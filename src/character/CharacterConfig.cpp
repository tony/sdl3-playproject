#include "character/CharacterConfig.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

#include <toml++/toml.h>
#include "util/TomlUtil.h"

static ActionTrigger parseTrigger(std::string_view s) {
  if (s == "press") {
    return ActionTrigger::Press;
  }
  return ActionTrigger::Hold;
}

static ActionInput parseInput(std::string_view s) {
  if (s == "action1") {
    return ActionInput::Action1;
  }
  if (s == "jump") {
    return ActionInput::Jump;
  }
  return ActionInput::Action2;
}

static bool parseJumpModel(std::string_view s, CharacterConfig::JumpModel& out) {
  if (s == "impulse") {
    out = CharacterConfig::JumpModel::Impulse;
    return true;
  }
  if (s == "dual_gravity") {
    out = CharacterConfig::JumpModel::DualGravity;
    return true;
  }
  if (s == "clamp_vy") {
    out = CharacterConfig::JumpModel::ClampVy;
    return true;
  }
  if (s == "fixed") {
    out = CharacterConfig::JumpModel::Fixed;
    return true;
  }
  return false;
}

static bool parseMoveModel(std::string_view s, CharacterConfig::MoveModel& out) {
  if (s == "approach") {
    out = CharacterConfig::MoveModel::Approach;
    return true;
  }
  if (s == "instant") {
    out = CharacterConfig::MoveModel::Instant;
    return true;
  }
  return false;
}

static bool parseCollisionSnap(std::string_view s, CharacterConfig::CollisionSnap& out) {
  if (s == "subpixel") {
    out = CharacterConfig::CollisionSnap::Subpixel;
    return true;
  }
  if (s == "pixel") {
    out = CharacterConfig::CollisionSnap::Pixel;
    return true;
  }
  return false;
}

static bool parseQuantizeMode(std::string_view s, CharacterConfig::Math::QuantizeMode& out) {
  if (s == "round") {
    out = CharacterConfig::Math::QuantizeMode::Round;
    return true;
  }
  if (s == "trunc") {
    out = CharacterConfig::Math::QuantizeMode::Trunc;
    return true;
  }
  return false;
}

static std::vector<std::string> parseStringArray(const toml::array& arr) {
  std::vector<std::string> out;
  out.reserve(arr.size());
  for (const auto& node : arr) {
    if (auto v = node.value<std::string>()) {
      out.push_back(*v);
    }
  }
  return out;
}

static bool parseVec2(const toml::node& node, CharacterConfig::FormStyle::AnchorDelta& out) {
  if (const auto* arr = node.as_array()) {
    if (arr->size() < 2) {
      return false;
    }
    auto x = (*arr)[0].value<double>();
    auto y = (*arr)[1].value<double>();
    if (!x || !y) {
      return false;
    }
    out.x = static_cast<float>(*x);
    out.y = static_cast<float>(*y);
    return true;
  }
  if (const auto* tbl = node.as_table()) {
    const auto* x = tbl->get("x");
    const auto* y = tbl->get("y");
    if (x == nullptr || y == nullptr) {
      return false;
    }
    auto xv = x->value<double>();
    auto yv = y->value<double>();
    if (!xv || !yv) {
      return false;
    }
    out.x = static_cast<float>(*xv);
    out.y = static_cast<float>(*yv);
    return true;
  }
  return false;
}

static CharacterConfig::RenderClip makeRenderClip(int row,
                                                  int start,
                                                  int frames,
                                                  float fps,
                                                  float rotateDeg = 0.0F) {
  CharacterConfig::RenderClip clip{};
  clip.row = row;
  clip.start = start;
  clip.frames = frames;
  clip.fps = fps;
  clip.rotateDeg = rotateDeg;
  return clip;
}

static const std::unordered_map<std::string, CharacterConfig::RenderClip>& defaultRenderAnims() {
  static const std::unordered_map<std::string, CharacterConfig::RenderClip> kDefaults = {
      {"idle", makeRenderClip(0, 0, 1, 0.0F)},
      {"run", makeRenderClip(1, 0, 4, 10.0F)},
      {"jump", makeRenderClip(2, 0, 1, 0.0F)},
      {"fall", makeRenderClip(3, 0, 1, 0.0F)},
      {"dash", makeRenderClip(4, 0, 1, 0.0F)},
      {"glide", makeRenderClip(5, 0, 1, 0.0F)},
      {"spindash_charge", makeRenderClip(6, 0, 4, 16.0F)},
  };
  return kDefaults;
}

// NOLINTBEGIN(readability-function-cognitive-complexity, readability-braces-around-statements,
// readability-qualified-auto, readability-implicit-bool-conversion)
bool CharacterConfig::loadFromToml(const char* path) {
  if (path == nullptr || path[0] == '\0') {
    return false;
  }

  std::unordered_set<std::string> seen;
  auto appendFromToml = [&](auto&& self, const std::filesystem::path& filePath) -> bool {
    const std::filesystem::path normalized = filePath.lexically_normal();
    const std::string pathStr = normalized.string();
    if (pathStr.empty()) {
      return false;
    }

    if (!seen.insert(pathStr).second) {
      TomlUtil::warnf(pathStr.c_str(), "character config include cycle detected; skipping");
      return true;
    }

    toml::table tbl;
    try {
      tbl = toml::parse_file(pathStr);
    } catch (...) {
      return false;
    }

    TomlUtil::warnUnknownKeys(tbl, pathStr.c_str(), "root",
                              {"version", "name", "render", "form", "powerups", "physics", "move",
                               "math", "jump", "collision", "states", "actions", "include"});

    if (auto include = tbl["include"].value<std::string>()) {
      const std::filesystem::path includePath = normalized.parent_path() / *include;
      if (!self(self, includePath)) {
        return false;
      }
    } else if (auto includes = tbl["include"].as_array()) {
      std::size_t idx = 0;
      for (const auto& node : *includes) {
        if (auto includePathStr = node.value<std::string>()) {
          const std::filesystem::path includePath = normalized.parent_path() / *includePathStr;
          if (!self(self, includePath)) {
            return false;
          }
        } else {
          TomlUtil::warnf(pathStr.c_str(), "include[{}] must be a string path", idx);
        }
        ++idx;
      }
    } else if (tbl.contains("include")) {
      TomlUtil::warnf(pathStr.c_str(), "include must be a string path or array of paths");
    }

    const char* path = pathStr.c_str();

    if (auto v = tbl["version"].value<int>())
      version = *v;

    if (auto n = tbl["name"].as_table()) {
      TomlUtil::warnUnknownKeys(*n, path, "name", {"id", "display"});
      if (auto v = n->get("id"))
        id = v->value_or(id);
      if (auto v = n->get("display"))
        displayName = v->value_or(displayName);
    }

    if (displayName.empty())
      displayName = id;

    if (auto r = tbl["render"].as_table()) {
      const auto& defaultAnims = defaultRenderAnims();
      if (render.anims.empty()) {
        render.anims = defaultAnims;
      }
      TomlUtil::warnUnknownKeys(*r, path, "render",
                                {"sheet", "sheets", "icon", "frame_w", "frame_h", "scale",
                                 "offset_x", "offset_y", "prefer_form", "anims"});
      if (auto v = r->get("sheet"))
        render.sheet = v->value_or(render.sheet);
      if (auto v = r->get("icon"))
        render.icon = v->value_or(render.icon);
      if (auto sheetsTable = (*r)["sheets"].as_table()) {
        for (const auto& [dir, val] : *sheetsTable) {
          if (auto s = val.value<std::string>()) {
            render.sheets[std::string{dir.str()}] = *s;
          }
        }
      }
      if (auto v = r->get("frame_w"))
        render.frameW = v->value_or(render.frameW);
      if (auto v = r->get("frame_h"))
        render.frameH = v->value_or(render.frameH);
      if (auto v = r->get("scale"))
        render.scale = v->value_or(render.scale);
      if (auto v = r->get("offset_x"))
        render.offsetX = v->value_or(render.offsetX);
      if (auto v = r->get("offset_y"))
        render.offsetY = v->value_or(render.offsetY);
      if (auto v = r->get("prefer_form"))
        render.preferForm = v->value_or(render.preferForm);

      if (auto anims = (*r)["anims"].as_table()) {
        for (const auto& [key, node] : *anims) {
          auto t = node.as_table();
          if (!t)
            continue;

          const std::string animName{key.str()};
          const std::string scope = "render.anims." + animName;
          TomlUtil::warnUnknownKeys(*t, path, scope,
                                    {"row", "start", "frames", "fps", "rotate_deg"});

          RenderClip clip{};
          if (auto it = defaultAnims.find(animName); it != defaultAnims.end()) {
            clip = it->second;
          }
          if (auto v = t->get("row"))
            clip.row = v->value_or(clip.row);
          if (auto v = t->get("start"))
            clip.start = v->value_or(clip.start);
          if (auto v = t->get("frames"))
            clip.frames = v->value_or(clip.frames);
          if (auto v = t->get("fps"))
            clip.fps = v->value_or(clip.fps);
          if (auto v = t->get("rotate_deg"))
            clip.rotateDeg = v->value_or(clip.rotateDeg);
          render.anims[animName] = clip;
        }
      }
    }

    if (auto f = tbl["form"].as_table()) {
      TomlUtil::warnUnknownKeys(
          *f, path, "form",
          {"id", "palette_base", "palette_accent", "variants", "colors", "anchor_deltas"});
      if (auto v = f->get("id"))
        form.id = v->value_or(form.id);
      if (auto v = f->get("palette_base"))
        form.paletteBase = v->value_or(form.paletteBase);
      if (auto v = f->get("palette_accent"))
        form.paletteAccent = v->value_or(form.paletteAccent);
      if (auto v = f->get("variants"); v != nullptr && v->is_array()) {
        form.variants = parseStringArray(*v->as_array());
      }
      if (auto colors = f->get("colors"); colors != nullptr && colors->is_table()) {
        for (const auto& [key, node] : *colors->as_table()) {
          if (auto value = node.value<std::string>()) {
            form.colors[std::string{key.str()}] = *value;
          }
        }
      }
      if (auto deltas = f->get("anchor_deltas"); deltas != nullptr && deltas->is_table()) {
        for (const auto& [key, node] : *deltas->as_table()) {
          const std::string anchorName{key.str()};
          CharacterConfig::FormStyle::AnchorDelta delta{};
          if (parseVec2(node, delta)) {
            form.anchorDeltas[anchorName] = delta;
          } else {
            TomlUtil::warnf(path, "form.anchor_deltas.{} must be [x,y] or {{x=,y=}}; skipping",
                            anchorName);
          }
        }
      }
    }

    if (auto p = tbl["powerups"].value<std::string>()) {
      if (!p->empty()) {
        powerups.push_back(*p);
      }
    } else if (auto p = tbl["powerups"].as_array()) {
      for (const auto& node : *p) {
        if (auto value = node.value<std::string>()) {
          if (!value->empty()) {
            powerups.push_back(*value);
          }
        } else {
          TomlUtil::warnf(path, "powerups entry must be a string; skipping");
        }
      }
    } else if (tbl.contains("powerups")) {
      TomlUtil::warnf(path, "powerups must be a string or array of strings");
    }

    if (auto p = tbl["physics"].as_table()) {
      TomlUtil::warnUnknownKeys(*p, path, "physics",
                                {"gravity", "max_fall_speed", "ground_friction", "air_drag"});
      if (auto v = p->get("gravity"))
        physics.gravity = v->value_or(physics.gravity);
      if (auto v = p->get("max_fall_speed"))
        physics.maxFallSpeed = v->value_or(physics.maxFallSpeed);
      if (auto v = p->get("ground_friction"))
        physics.groundFriction = v->value_or(physics.groundFriction);
      if (auto v = p->get("air_drag"))
        physics.airDrag = v->value_or(physics.airDrag);
    }

    if (auto m = tbl["move"].as_table()) {
      TomlUtil::warnUnknownKeys(
          *m, path, "move",
          {"accel_ground", "accel_air", "decel_ground", "max_speed_ground", "max_speed_air",
           "turn_resistance", "turn_resistance_air", "air_control", "model"});
      if (auto v = m->get("model")) {
        const std::string s = v->value_or(std::string{});
        if (!s.empty()) {
          MoveModel parsed = MoveModel::Approach;
          if (!parseMoveModel(s, parsed)) {
            TomlUtil::warnf(path, "move.model must be one of: \"approach\", \"instant\" (got '{}')",
                            s);
          } else {
            move.model = parsed;
          }
        }
      }
      if (auto v = m->get("accel_ground"))
        move.accelGround = v->value_or(move.accelGround);
      if (auto v = m->get("accel_air"))
        move.accelAir = v->value_or(move.accelAir);
      if (auto v = m->get("air_control"))
        move.airControl = v->value_or(move.airControl);
      if (auto v = m->get("decel_ground"))
        move.decelGround = v->value_or(move.decelGround);
      if (auto v = m->get("max_speed_ground"))
        move.maxSpeedGround = v->value_or(move.maxSpeedGround);
      if (auto v = m->get("max_speed_air"))
        move.maxSpeedAir = v->value_or(move.maxSpeedAir);
      if (auto v = m->get("turn_resistance"))
        move.turnResistance = v->value_or(move.turnResistance);
      move.turnResistanceAir = move.turnResistance;
      if (auto v = m->get("turn_resistance_air"))
        move.turnResistanceAir = v->value_or(move.turnResistanceAir);
    }

    if (auto m = tbl["math"].as_table()) {
      TomlUtil::warnUnknownKeys(*m, path, "math", {"subpixel", "collision_snap", "quantize"});
      if (auto v = m->get("subpixel")) {
        const int next = v->value_or(math.subpixel);
        if (next >= 0)
          math.subpixel = next;
      }
      if (auto v = m->get("collision_snap")) {
        const std::string s = v->value_or(std::string{});
        if (!s.empty()) {
          CollisionSnap parsed = CollisionSnap::Subpixel;
          if (!parseCollisionSnap(s, parsed)) {
            TomlUtil::warnf(
                path, "math.collision_snap must be one of: \"subpixel\", \"pixel\" (got '{}')", s);
          } else {
            math.collisionSnap = parsed;
          }
        }
      }

      if (auto v = m->get("quantize")) {
        const std::string s = v->value_or(std::string{});
        if (!s.empty()) {
          Math::QuantizeMode parsed = Math::QuantizeMode::Round;
          if (!parseQuantizeMode(s, parsed)) {
            TomlUtil::warnf(path, "math.quantize must be one of: \"round\", \"trunc\" (got '{}')",
                            s);
          } else {
            math.quantize = parsed;
          }
        }
      }
    }

    auto loadEnvironmentMultipliers = [&](const toml::table& t, const std::string& scope,
                                          CharacterConfig::EnvironmentMultipliers& out) {
      TomlUtil::warnUnknownKeys(
          t, path, scope,
          {"gravity_multiplier", "max_fall_speed_multiplier", "accel_multiplier",
           "max_speed_multiplier", "friction_multiplier", "ground_friction_multiplier",
           "air_drag_multiplier", "turn_resistance_multiplier", "jump_impulse_multiplier"});

      auto loadScale = [&](const char* key, float& target) {
        if (auto v = t.get(key)) {
          const float next = v->value_or(target);
          if (next >= 0.0F) {
            target = next;
          } else {
            TomlUtil::warnf(path, "{}.{} must be >= 0 (got {:.2f}); using default", scope.c_str(),
                            key, next);
          }
        }
      };

      loadScale("gravity_multiplier", out.gravity);
      loadScale("max_fall_speed_multiplier", out.maxFallSpeed);
      loadScale("accel_multiplier", out.accel);
      loadScale("max_speed_multiplier", out.maxSpeed);
      loadScale("friction_multiplier", out.friction);
      loadScale("ground_friction_multiplier", out.groundFriction);
      loadScale("air_drag_multiplier", out.airDrag);
      loadScale("turn_resistance_multiplier", out.turnResistance);
      loadScale("jump_impulse_multiplier", out.jumpImpulse);
    };

    if (auto env = tbl["environment"].as_table()) {
      TomlUtil::warnUnknownKeys(*env, path, "environment", {"water", "ice"});
      if (auto w = (*env)["water"].as_table()) {
        loadEnvironmentMultipliers(*w, "environment.water", environment.water);
      }
      if (auto i = (*env)["ice"].as_table()) {
        loadEnvironmentMultipliers(*i, "environment.ice", environment.ice);
      }
    }

    if (auto j = tbl["jump"].as_table()) {
      TomlUtil::warnUnknownKeys(
          *j, path, "jump",
          {"enabled", "model", "impulse", "coyote_frames", "jump_buffer_frames", "variable_jump",
           "variable_cut_multiplier", "fall_gravity_multiplier", "max_hold_frames",
           "impulse_by_speed", "rise_gravity_multiplier_held", "rise_gravity_multiplier_released",
           "release_clamp_vy", "release_drop_after_frames", "release_drop"});
      if (auto v = j->get("enabled"))
        jump.enabled = v->value_or(jump.enabled);
      if (auto v = j->get("model")) {
        const std::string s = v->value_or(std::string{});
        if (!s.empty()) {
          JumpModel parsed = JumpModel::Impulse;
          if (!parseJumpModel(s, parsed)) {
            TomlUtil::warnf(
                path,
                "jump.model must be one of: \"impulse\", \"dual_gravity\", \"clamp_vy\", "
                "\"fixed\" (got '{}')",
                s);
          } else {
            jump.model = parsed;
          }
        }
      }
      if (auto v = j->get("impulse"))
        jump.impulse = v->value_or(jump.impulse);
      if (auto v = j->get("coyote_frames"))
        jump.coyoteFrames = v->value_or(jump.coyoteFrames);
      if (auto v = j->get("jump_buffer_frames"))
        jump.jumpBufferFrames = v->value_or(jump.jumpBufferFrames);
      if (auto v = j->get("max_hold_frames")) {
        const int next = v->value_or(jump.maxHoldFrames);
        if (next >= 0)
          jump.maxHoldFrames = next;
      }
      if (auto v = j->get("variable_jump"))
        jump.variableJump = v->value_or(jump.variableJump);
      if (auto v = j->get("variable_cut_multiplier"))
        jump.variableCutMultiplier = v->value_or(jump.variableCutMultiplier);
      if (auto v = j->get("fall_gravity_multiplier"))
        jump.fallGravityMultiplier = v->value_or(jump.fallGravityMultiplier);

      if (auto a = (*j)["impulse_by_speed"].as_array()) {
        jump.impulseBySpeed.clear();
        jump.impulseBySpeed.reserve(a->size());
        for (const auto& node : *a) {
          auto t = node.as_table();
          if (!t)
            continue;

          TomlUtil::warnUnknownKeys(*t, path, "jump.impulse_by_speed", {"min_abs_vx", "impulse"});

          Jump::ImpulseBySpeed e{};
          if (auto v = t->get("min_abs_vx"))
            e.minAbsVx = v->value_or(e.minAbsVx);
          if (auto v = t->get("impulse"))
            e.impulse = v->value_or(e.impulse);

          if (e.minAbsVx < 0.0F) {
            TomlUtil::warnf(path,
                            "jump.impulse_by_speed.min_abs_vx must be >= 0 (got {:.2f}); skipping",
                            e.minAbsVx);
            continue;
          }
          if (e.impulse <= 0.0F) {
            TomlUtil::warnf(path,
                            "jump.impulse_by_speed.impulse must be > 0 (got {:.2f}); skipping",
                            e.impulse);
            continue;
          }

          jump.impulseBySpeed.push_back(e);
        }

        std::sort(jump.impulseBySpeed.begin(), jump.impulseBySpeed.end(),
                  [](const Jump::ImpulseBySpeed& a, const Jump::ImpulseBySpeed& b) {
                    return a.minAbsVx < b.minAbsVx;
                  });
      }
      if (auto v = j->get("rise_gravity_multiplier_held"))
        jump.riseGravityMultiplierHeld = v->value_or(jump.riseGravityMultiplierHeld);
      if (auto v = j->get("rise_gravity_multiplier_released"))
        jump.riseGravityMultiplierReleased = v->value_or(jump.riseGravityMultiplierReleased);
      if (auto v = j->get("release_clamp_vy")) {
        const float next = v->value_or(jump.releaseClampVy);
        if (next >= 0.0F)
          jump.releaseClampVy = next;
      }
      if (auto v = j->get("release_drop_after_frames")) {
        const int next = v->value_or(jump.releaseDropAfterFrames);
        if (next >= 0)
          jump.releaseDropAfterFrames = next;
      }
      if (auto v = j->get("release_drop")) {
        const float next = v->value_or(jump.releaseDrop);
        if (next >= 0.0F)
          jump.releaseDrop = next;
      }
    }

    if (auto c = tbl["collision"].as_table()) {
      TomlUtil::warnUnknownKeys(*c, path, "collision", {"w", "h"});
      if (auto v = c->get("w")) {
        const float next = v->value_or(collision.w);
        if (next > 0.0F)
          collision.w = next;
      }
      if (auto v = c->get("h")) {
        const float next = v->value_or(collision.h);
        if (next > 0.0F)
          collision.h = next;
      }
    }

    if (auto s = tbl["states"].as_table()) {
      TomlUtil::warnUnknownKeys(*s, path, "states", {"rolling", "drop_through", "slide"});

      if (auto r = (*s)["rolling"].as_table()) {
        TomlUtil::warnUnknownKeys(
            *r, path, "states.rolling",
            {"enabled", "auto_start_after_spindash", "hitbox_w", "hitbox_h", "accel_multiplier",
             "max_speed_multiplier", "friction_multiplier", "stop_speed", "disable_air_control",
             "allow_jump"});
        if (auto v = r->get("enabled"))
          states.rolling.enabled = v->value_or(states.rolling.enabled);
        if (auto v = r->get("auto_start_after_spindash"))
          states.rolling.autoStartAfterSpindash =
              v->value_or(states.rolling.autoStartAfterSpindash);
        if (auto v = r->get("hitbox_w")) {
          const float next = v->value_or(states.rolling.hitboxW);
          if (next >= 0.0F)
            states.rolling.hitboxW = next;
        }
        if (auto v = r->get("hitbox_h")) {
          const float next = v->value_or(states.rolling.hitboxH);
          if (next >= 0.0F)
            states.rolling.hitboxH = next;
        }
        if (auto v = r->get("accel_multiplier")) {
          const float next = v->value_or(states.rolling.accelMultiplier);
          if (next >= 0.0F) {
            states.rolling.accelMultiplier = next;
          } else {
            TomlUtil::warnf(
                path, "states.rolling.accel_multiplier must be >= 0 (got {:.2f}); using default",
                next);
          }
        }
        if (auto v = r->get("max_speed_multiplier")) {
          const float next = v->value_or(states.rolling.maxSpeedMultiplier);
          if (next >= 0.0F) {
            states.rolling.maxSpeedMultiplier = next;
          } else {
            TomlUtil::warnf(
                path,
                "states.rolling.max_speed_multiplier must be >= 0 (got {:.2f}); using default",
                next);
          }
        }
        if (auto v = r->get("friction_multiplier")) {
          const float next = v->value_or(states.rolling.frictionMultiplier);
          states.rolling.frictionMultiplier = std::clamp(next, 0.0F, 2.0F);
          if (states.rolling.frictionMultiplier != next) {
            TomlUtil::warnf(
                path,
                "states.rolling.friction_multiplier should be in [0,2] (got {:.2f}); clamping",
                next);
          }
        }
        if (auto v = r->get("stop_speed")) {
          const float next = v->value_or(states.rolling.stopSpeed);
          if (next >= 0.0F)
            states.rolling.stopSpeed = next;
        }
        if (auto v = r->get("disable_air_control"))
          states.rolling.disableAirControl = v->value_or(states.rolling.disableAirControl);
        if (auto v = r->get("allow_jump"))
          states.rolling.allowJump = v->value_or(states.rolling.allowJump);
      }

      if (auto d = (*s)["drop_through"].as_table()) {
        TomlUtil::warnUnknownKeys(*d, path, "states.drop_through", {"hold_frames"});
        if (auto v = d->get("hold_frames")) {
          const int next = v->value_or(states.dropThrough.holdFrames);
          if (next >= 0) {
            states.dropThrough.holdFrames = next;
          } else {
            TomlUtil::warnf(
                path, "states.drop_through.hold_frames must be >= 0 (got {}); using default", next);
          }
        }
      }

      if (auto sl = (*s)["slide"].as_table()) {
        TomlUtil::warnUnknownKeys(
            *sl, path, "states.slide",
            {"enabled", "duration_frames", "jump_lockout_frames", "allow_jump_cancel", "hitbox_w",
             "hitbox_h", "slide_speed", "cancel_on_ground_loss", "stay_low_under_ceiling",
             "input_window_frames"});
        if (auto v = sl->get("enabled"))
          states.slide.enabled = v->value_or(states.slide.enabled);
        if (auto v = sl->get("duration_frames")) {
          const int next = v->value_or(states.slide.durationFrames);
          if (next > 0)
            states.slide.durationFrames = next;
          else
            TomlUtil::warnf(
                path, "states.slide.duration_frames must be > 0 (got {}); using default", next);
        }
        if (auto v = sl->get("jump_lockout_frames")) {
          const int next = v->value_or(states.slide.jumpLockoutFrames);
          if (next >= 0)
            states.slide.jumpLockoutFrames = next;
        }
        if (auto v = sl->get("allow_jump_cancel"))
          states.slide.allowJumpCancel = v->value_or(states.slide.allowJumpCancel);
        if (auto v = sl->get("input_window_frames")) {
          const int next = v->value_or(states.slide.inputWindowFrames);
          if (next >= 0)
            states.slide.inputWindowFrames = next;
          else
            TomlUtil::warnf(path,
                            "states.slide.input_window_frames must be >= 0 (got {}); using default",
                            next);
        }
        if (auto v = sl->get("hitbox_w")) {
          const float next = v->value_or(states.slide.hitboxW);
          if (next >= 0.0F)
            states.slide.hitboxW = next;
        }
        if (auto v = sl->get("hitbox_h")) {
          const float next = v->value_or(states.slide.hitboxH);
          if (next >= 0.0F)
            states.slide.hitboxH = next;
        }
        if (auto v = sl->get("slide_speed")) {
          const float next = v->value_or(states.slide.slideSpeed);
          if (next > 0.0F)
            states.slide.slideSpeed = next;
          else
            TomlUtil::warnf(
                path, "states.slide.slide_speed must be > 0 (got {:.2f}); using default", next);
        }
        if (auto v = sl->get("cancel_on_ground_loss"))
          states.slide.cancelOnGroundLoss = v->value_or(states.slide.cancelOnGroundLoss);
        if (auto v = sl->get("stay_low_under_ceiling"))
          states.slide.stayLowUnderCeiling = v->value_or(states.slide.stayLowUnderCeiling);
      }
    }

    if (auto a = tbl["actions"].as_table()) {
      TomlUtil::warnUnknownKeys(
          *a, path, "actions",
          {"spin", "dash", "fly", "glide", "spindash", "wall", "attack", "attacks"});
      if (auto s = (*a)["spin"].as_table()) {
        TomlUtil::warnUnknownKeys(*s, path, "actions.spin",
                                  {"enabled", "type", "input", "min_speed", "spin_friction"});
        if (auto v = s->get("enabled"))
          actions.spin.enabled = v->value_or(actions.spin.enabled);
        if (auto v = s->get("min_speed"))
          actions.spin.minSpeed = v->value_or(actions.spin.minSpeed);
        if (auto v = s->get("spin_friction"))
          actions.spin.spinFriction = v->value_or(actions.spin.spinFriction);
        if (auto v = s->get("input"))
          actions.spin.input = parseInput(v->value_or(std::string_view{"action1"}));
        if (auto v = s->get("type"))
          actions.spin.trigger = parseTrigger(v->value_or(std::string_view{"hold"}));
      }
      if (auto d = (*a)["dash"].as_table()) {
        TomlUtil::warnUnknownKeys(
            *d, path, "actions.dash",
            {"enabled", "type", "input", "hold_frames", "cooldown_frames", "dash_time_frames",
             "dash_speed", "allow_air", "air_dashes", "allow_jump_override", "preserve_momentum"});
        if (auto v = d->get("enabled"))
          actions.dash.enabled = v->value_or(actions.dash.enabled);
        if (auto v = d->get("hold_frames")) {
          const int next = v->value_or(actions.dash.holdFrames);
          if (next >= 0)
            actions.dash.holdFrames = next;
          else
            TomlUtil::warnf(path, "actions.dash.hold_frames must be >= 0 (got {}); using default",
                            next);
        }
        if (auto v = d->get("cooldown_frames"))
          actions.dash.cooldownFrames = v->value_or(actions.dash.cooldownFrames);
        if (auto v = d->get("dash_time_frames"))
          actions.dash.dashTimeFrames = v->value_or(actions.dash.dashTimeFrames);
        if (auto v = d->get("dash_speed"))
          actions.dash.dashSpeed = v->value_or(actions.dash.dashSpeed);
        if (auto v = d->get("allow_air"))
          actions.dash.allowAir = v->value_or(actions.dash.allowAir);
        if (auto v = d->get("air_dashes"))
          actions.dash.airDashes = v->value_or(actions.dash.airDashes);
        if (auto v = d->get("allow_jump_override"))
          actions.dash.allowJumpOverride = v->value_or(actions.dash.allowJumpOverride);
        if (auto v = d->get("preserve_momentum"))
          actions.dash.preserveMomentum = v->value_or(actions.dash.preserveMomentum);
        if (auto v = d->get("input"))
          actions.dash.input = parseInput(v->value_or(std::string_view{"action1"}));
        if (auto v = d->get("type"))
          actions.dash.trigger = parseTrigger(v->value_or(std::string_view{"press"}));
      }
      if (auto f = (*a)["fly"].as_table()) {
        TomlUtil::warnUnknownKeys(
            *f, path, "actions.fly",
            {"enabled", "type", "input", "up_accel", "max_up_speed", "allow_jump_override"});
        if (auto v = f->get("enabled"))
          actions.fly.enabled = v->value_or(actions.fly.enabled);
        if (auto v = f->get("up_accel"))
          actions.fly.upAccel = v->value_or(actions.fly.upAccel);
        if (auto v = f->get("max_up_speed"))
          actions.fly.maxUpSpeed = v->value_or(actions.fly.maxUpSpeed);
        if (auto v = f->get("allow_jump_override"))
          actions.fly.allowJumpOverride = v->value_or(actions.fly.allowJumpOverride);
        if (auto v = f->get("input"))
          actions.fly.input = parseInput(v->value_or(std::string_view{"action2"}));
        if (auto v = f->get("type"))
          actions.fly.trigger = parseTrigger(v->value_or(std::string_view{"hold"}));
      }
      if (auto g = (*a)["glide"].as_table()) {
        TomlUtil::warnUnknownKeys(*g, path, "actions.glide",
                                  {"enabled", "type", "input", "gravity_multiplier",
                                   "max_fall_speed", "start_on_press", "allow_jump_override"});
        if (auto v = g->get("enabled"))
          actions.glide.enabled = v->value_or(actions.glide.enabled);
        if (auto v = g->get("gravity_multiplier"))
          actions.glide.gravityMultiplier = v->value_or(actions.glide.gravityMultiplier);
        if (auto v = g->get("max_fall_speed"))
          actions.glide.maxFallSpeed = v->value_or(actions.glide.maxFallSpeed);
        if (auto v = g->get("input"))
          actions.glide.input = parseInput(v->value_or(std::string_view{"action2"}));
        if (auto v = g->get("start_on_press"))
          actions.glide.startOnPress = v->value_or(actions.glide.startOnPress);
        if (auto v = g->get("allow_jump_override"))
          actions.glide.allowJumpOverride = v->value_or(actions.glide.allowJumpOverride);
        if (auto v = g->get("type"))
          actions.glide.trigger = parseTrigger(v->value_or(std::string_view{"hold"}));
      }
      if (auto sd = (*a)["spindash"].as_table()) {
        TomlUtil::warnUnknownKeys(
            *sd, path, "actions.spindash",
            {"enabled", "type", "input", "require_down", "down_window_frames", "charge_frames",
             "min_launch_speed", "max_launch_speed", "tap_boost_frames"});
        if (auto v = sd->get("enabled"))
          actions.spindash.enabled = v->value_or(actions.spindash.enabled);
        if (auto v = sd->get("require_down"))
          actions.spindash.requireDown = v->value_or(actions.spindash.requireDown);
        if (auto v = sd->get("down_window_frames")) {
          const int next = v->value_or(actions.spindash.downWindowFrames);
          if (next >= 0)
            actions.spindash.downWindowFrames = next;
          else
            TomlUtil::warnf(
                path, "actions.spindash.down_window_frames must be >= 0 (got {}); using default",
                next);
        }
        if (auto v = sd->get("charge_frames"))
          actions.spindash.chargeFrames = v->value_or(actions.spindash.chargeFrames);
        if (auto v = sd->get("min_launch_speed"))
          actions.spindash.minLaunchSpeed = v->value_or(actions.spindash.minLaunchSpeed);
        if (auto v = sd->get("max_launch_speed"))
          actions.spindash.maxLaunchSpeed = v->value_or(actions.spindash.maxLaunchSpeed);
        if (auto v = sd->get("tap_boost_frames"))
          actions.spindash.tapBoostFrames = v->value_or(actions.spindash.tapBoostFrames);
        if (auto v = sd->get("input"))
          actions.spindash.input = parseInput(v->value_or(std::string_view{"action1"}));
        if (auto v = sd->get("type"))
          actions.spindash.trigger = parseTrigger(v->value_or(std::string_view{"hold"}));
      }

      if (auto wall = (*a)["wall"].as_table()) {
        TomlUtil::warnUnknownKeys(
            *wall, path, "actions.wall",
            {"enabled", "probe", "require_input", "slide_gravity_multiplier",
             "slide_max_fall_speed", "climb_speed", "descend_speed", "jump_impulse", "jump_vx",
             "detach_frames", "dash_kick_enabled", "dash_kick_speed"});
        if (auto v = wall->get("enabled"))
          actions.wall.enabled = v->value_or(actions.wall.enabled);
        if (auto v = wall->get("probe")) {
          const float next = v->value_or(actions.wall.probe);
          if (next >= 0.0F)
            actions.wall.probe = next;
          else
            TomlUtil::warnf(path, "actions.wall.probe must be >= 0 (got {:.2f}); clamping to 0",
                            next);
        }
        if (auto v = wall->get("require_input"))
          actions.wall.requireInput = v->value_or(actions.wall.requireInput);
        if (auto v = wall->get("slide_gravity_multiplier")) {
          const float next = v->value_or(actions.wall.slideGravityMultiplier);
          actions.wall.slideGravityMultiplier = std::clamp(next, 0.0F, 1.0F);
          if (actions.wall.slideGravityMultiplier != next) {
            TomlUtil::warnf(
                path,
                "actions.wall.slide_gravity_multiplier must be in [0,1] (got {:.2f}); clamping",
                next);
          }
        }
        if (auto v = wall->get("slide_max_fall_speed")) {
          const float next = v->value_or(actions.wall.slideMaxFallSpeed);
          if (next >= 0.0F)
            actions.wall.slideMaxFallSpeed = next;
          else
            TomlUtil::warnf(
                path, "actions.wall.slide_max_fall_speed must be >= 0 (got {:.2f}); clamping to 0",
                next);
        }
        if (auto v = wall->get("climb_speed")) {
          const float next = v->value_or(actions.wall.climbSpeed);
          if (next >= 0.0F)
            actions.wall.climbSpeed = next;
          else
            TomlUtil::warnf(
                path, "actions.wall.climb_speed must be >= 0 (got {:.2f}); clamping to 0", next);
        }
        if (auto v = wall->get("descend_speed")) {
          const float next = v->value_or(actions.wall.descendSpeed);
          if (next >= 0.0F)
            actions.wall.descendSpeed = next;
          else
            TomlUtil::warnf(
                path, "actions.wall.descend_speed must be >= 0 (got {:.2f}); clamping to 0", next);
        }
        if (auto v = wall->get("jump_impulse")) {
          const float next = v->value_or(actions.wall.jumpImpulse);
          if (next >= 0.0F)
            actions.wall.jumpImpulse = next;
          else
            TomlUtil::warnf(
                path, "actions.wall.jump_impulse must be >= 0 (got {:.2f}); clamping to 0", next);
        }
        if (auto v = wall->get("jump_vx")) {
          const float next = v->value_or(actions.wall.jumpVx);
          if (next >= 0.0F)
            actions.wall.jumpVx = next;
          else
            TomlUtil::warnf(path, "actions.wall.jump_vx must be >= 0 (got {:.2f}); clamping to 0",
                            next);
        }
        if (auto v = wall->get("detach_frames")) {
          const int next = v->value_or(actions.wall.detachFrames);
          if (next >= 0)
            actions.wall.detachFrames = next;
          else
            TomlUtil::warnf(path, "actions.wall.detach_frames must be >= 0 (got {}); clamping to 0",
                            next);
        }
        if (auto v = wall->get("dash_kick_enabled"))
          actions.wall.dashKickEnabled = v->value_or(actions.wall.dashKickEnabled);
        if (auto v = wall->get("dash_kick_speed")) {
          const float next = v->value_or(actions.wall.dashKickSpeed);
          if (next >= 0.0F)
            actions.wall.dashKickSpeed = next;
          else
            TomlUtil::warnf(path,
                            "actions.wall.dash_kick_speed must be >= 0 (got {:.2f}); clamping to 0",
                            next);
        }
      }

      auto parseAttackTable = [&](const toml::table& at, const std::string& scope) {
        TomlUtil::warnUnknownKeys(
            at, path, scope,
            {"id", "enabled", "type", "input", "allow_air", "startup_frames", "active_frames",
             "cooldown_frames", "hits", "damage", "knockback_vx", "knockback_vy", "hitbox_w",
             "hitbox_h", "offset_x", "offset_y", "powerup", "projectile"});

        AttackAction attack{};
        if (auto v = at.get("id"))
          attack.id = v->value_or(attack.id);
        if (auto v = at.get("enabled"))
          attack.enabled = v->value_or(attack.enabled);
        if (auto v = at.get("type"))
          attack.trigger = parseTrigger(v->value_or(std::string_view{"press"}));
        if (auto v = at.get("input"))
          attack.input = parseInput(v->value_or(std::string_view{"action1"}));
        if (auto v = at.get("allow_air"))
          attack.allowAir = v->value_or(attack.allowAir);
        if (auto v = at.get("startup_frames")) {
          const int next = v->value_or(attack.startupFrames);
          if (next >= 0)
            attack.startupFrames = next;
          else
            TomlUtil::warnf(path, "{}.startup_frames must be >= 0 (got {}); using default", scope,
                            next);
        }
        if (auto v = at.get("active_frames")) {
          const int next = v->value_or(attack.activeFrames);
          if (next > 0)
            attack.activeFrames = next;
          else
            TomlUtil::warnf(path, "{}.active_frames must be > 0 (got {}); using default", scope,
                            next);
        }
        if (auto v = at.get("cooldown_frames")) {
          const int next = v->value_or(attack.cooldownFrames);
          if (next >= 0)
            attack.cooldownFrames = next;
          else
            TomlUtil::warnf(path, "{}.cooldown_frames must be >= 0 (got {}); using default", scope,
                            next);
        }
        if (auto v = at.get("hits")) {
          const int next = v->value_or(attack.hits);
          if (next > 0)
            attack.hits = next;
          else
            TomlUtil::warnf(path, "{}.hits must be > 0 (got {}); using default", scope, next);
        }
        if (auto v = at.get("damage"))
          attack.damage = v->value_or(attack.damage);
        if (auto v = at.get("knockback_vx"))
          attack.knockbackVx = v->value_or(attack.knockbackVx);
        if (auto v = at.get("knockback_vy"))
          attack.knockbackVy = v->value_or(attack.knockbackVy);
        if (auto v = at.get("hitbox_w")) {
          const float next = v->value_or(attack.hitbox.w);
          if (next > 0.0F)
            attack.hitbox.w = next;
          else
            TomlUtil::warnf(path, "{}.hitbox_w must be > 0 (got {:.2f}); using default", scope,
                            next);
        }
        if (auto v = at.get("hitbox_h")) {
          const float next = v->value_or(attack.hitbox.h);
          if (next > 0.0F)
            attack.hitbox.h = next;
          else
            TomlUtil::warnf(path, "{}.hitbox_h must be > 0 (got {:.2f}); using default", scope,
                            next);
        }
        if (auto v = at.get("offset_x"))
          attack.hitbox.offsetX = v->value_or(attack.hitbox.offsetX);
        if (auto v = at.get("offset_y"))
          attack.hitbox.offsetY = v->value_or(attack.hitbox.offsetY);
        if (auto v = at.get("powerup"))
          attack.powerupId = v->value_or(attack.powerupId);

        if (auto proj = at["projectile"].as_table()) {
          TomlUtil::warnUnknownKeys(
              *proj, path, scope + ".projectile",
              {"enabled", "speed", "gravity", "lifetime_frames", "w", "h", "offset_x", "offset_y",
               "damage", "knockback_vx", "knockback_vy"});
          if (auto v = proj->get("enabled"))
            attack.projectile.enabled = v->value_or(attack.projectile.enabled);
          if (auto v = proj->get("speed"))
            attack.projectile.speed = v->value_or(attack.projectile.speed);
          if (auto v = proj->get("gravity"))
            attack.projectile.gravity = v->value_or(attack.projectile.gravity);
          if (auto v = proj->get("lifetime_frames")) {
            const int next = v->value_or(attack.projectile.lifetimeFrames);
            if (next > 0)
              attack.projectile.lifetimeFrames = next;
            else
              TomlUtil::warnf(path,
                              "{}.projectile.lifetime_frames must be > 0 (got {}); using default",
                              scope, next);
          }
          if (auto v = proj->get("w")) {
            const float next = v->value_or(attack.projectile.w);
            if (next > 0.0F)
              attack.projectile.w = next;
          }
          if (auto v = proj->get("h")) {
            const float next = v->value_or(attack.projectile.h);
            if (next > 0.0F)
              attack.projectile.h = next;
          }
          if (auto v = proj->get("offset_x"))
            attack.projectile.offsetX = v->value_or(attack.projectile.offsetX);
          if (auto v = proj->get("offset_y"))
            attack.projectile.offsetY = v->value_or(attack.projectile.offsetY);
          if (auto v = proj->get("damage"))
            attack.projectile.damage = v->value_or(attack.projectile.damage);
          if (auto v = proj->get("knockback_vx"))
            attack.projectile.knockbackVx = v->value_or(attack.projectile.knockbackVx);
          if (auto v = proj->get("knockback_vy"))
            attack.projectile.knockbackVy = v->value_or(attack.projectile.knockbackVy);
        }

        actions.attacks.push_back(std::move(attack));
      };

      if (auto at = (*a)["attack"].as_table()) {
        parseAttackTable(*at, "actions.attack");
      }
      if (auto ats = (*a)["attacks"].as_array()) {
        std::size_t idx = 0;
        for (const auto& node : *ats) {
          auto t = node.as_table();
          ++idx;
          if (!t)
            continue;
          parseAttackTable(*t, "actions.attacks[" + std::to_string(idx - 1) + "]");
        }
      }
    }

    return true;
  };

  return appendFromToml(appendFromToml, path);
}
// NOLINTEND(readability-function-cognitive-complexity, readability-braces-around-statements,
// readability-qualified-auto, readability-implicit-bool-conversion)
