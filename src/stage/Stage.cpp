#include "stage/Stage.h"

#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_render.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <format>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <toml++/toml.h>
#include "character/CharacterConfig.h"
#include "character/CharacterController.h"
#include "collectible/CollectibleConfig.h"
#include "ecs/Components.h"
#include "ecs/World.h"
#include "enemy/EnemyConfig.h"
#include "util/Paths.h"
#include "util/TomlUtil.h"
#include "visual/Palette.h"
#include "visual/Shapes.h"

// Test stage defaults (used by loadTestStage fallback)
static constexpr float kTestGroundSnap = 2.0F;
static constexpr float kTestStepUp = 6.0F;
static constexpr float kTestCollisionSkin = 0.01F;
static constexpr float kTestWorldW = 2000.0F;
static constexpr float kTestWorldH = 600.0F;
static constexpr float kTestCameraW = 2000.0F;
static constexpr float kTestCameraH = 540.0F;
static constexpr float kTestDeadzoneW = 240.0F;
static constexpr float kTestDeadzoneH = 140.0F;
static constexpr float kTestLookaheadX = 120.0F;

// Debug rendering colors
static constexpr SDL_Color kDebugOneWay = {255, 255, 0, 255};    // yellow
static constexpr SDL_Color kDebugSolid = {255, 128, 0, 255};     // orange
static constexpr SDL_Color kDebugSlope = {0, 255, 128, 255};     // aqua
static constexpr SDL_Color kDebugWorld = {0, 160, 255, 255};     // blue
static constexpr SDL_Color kDebugCamera = {200, 120, 255, 255};  // purple
static constexpr SDL_Color kDebugWater = {64, 160, 255, 255};    // light blue
static constexpr SDL_Color kDebugIce = {200, 220, 255, 255};     // pale blue
static constexpr SDL_Color kDebugZone = {160, 160, 160, 255};    // gray
static constexpr SDL_Color kDebugHazard = {255, 0, 0, 255};      // red
static constexpr SDL_Color kDebugSpawn = {0, 255, 255, 255};     // cyan

static bool
aabbOverlap(float ax, float ay, float aw, float ah, float bx, float by, float bw, float bh) {
  return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

static float slopeSurfaceY(const SlopeRect& slope, float x) {
  const Rect& r = slope.rect;
  if (r.w <= 0.0F)
    return r.y;
  const float t = std::clamp((x - r.x) / r.w, 0.0F, 1.0F);
  switch (slope.dir) {
    case SlopeDir::UpRight:
      return r.y + r.h * (1.0F - t);
    case SlopeDir::UpLeft:
      return r.y + r.h * t;
  }
  return r.y;
}

static bool isHexDigit(char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static bool parseHexColorString(std::string_view s, Rgba8& out) {
  if (!s.empty() && s.front() == '#')
    s.remove_prefix(1);
  if (s.size() != 6)
    return false;
  for (const char c : s) {
    if (!isHexDigit(c))
      return false;
  }

  std::string tmp(s);
  const Visual::Color c = Visual::Color::fromHex(tmp.c_str());
  out = Rgba8{c.r, c.g, c.b, c.a};
  return true;
}

static Rgba8 rgbaFromHex(const char* hex) {
  const Visual::Color c = Visual::Color::fromHex(hex);
  return Rgba8{c.r, c.g, c.b, c.a};
}

static StageRenderStyle defaultRenderStyle() {
  StageRenderStyle s{};
  s.bgTop = rgbaFromHex("0C0C10");
  s.bgBottom = rgbaFromHex("1A1A28");
  s.platformBase = rgbaFromHex("4A4A52");
  s.platformLight = rgbaFromHex("6A6A74");
  s.platformDark = rgbaFromHex("2A2A32");
  s.platformHighlight = rgbaFromHex("8A8A94");
  return s;
}

static void maybeReadRenderColor(const char* stageTomlPath,
                                 const toml::table& render,
                                 const char* key,
                                 Rgba8& inOut) {
  const toml::node* node = render.get(key);
  if (!node)
    return;

  const auto s = node->value<std::string>();
  if (!s) {
    TomlUtil::warnf(stageTomlPath, "render.{} must be a hex string like \"RRGGBB\"", key);
    return;
  }
  if (!parseHexColorString(*s, inOut)) {
    TomlUtil::warnf(stageTomlPath, "render.{} must be a hex string like \"RRGGBB\" (got '{}')", key,
                    s->c_str());
  }
}

void Stage::loadTestStage() {
  solids_.clear();
  slopes_.clear();
  zones_.clear();
  hazards_.clear();
  cameraLocks_.clear();
  spawns_.clear();
  enemySpawns_.clear();
  collectibleSpawns_.clear();
  version_ = 1;
  id_ = "test_stage";
  displayName_ = "Test Stage";
  groundSnap_ = kTestGroundSnap;
  stepUp_ = kTestStepUp;
  collisionSkin_ = kTestCollisionSkin;
  hasWorldBounds_ = true;
  worldBounds_ = Rect{0.0F, 0.0F, kTestWorldW, kTestWorldH};
  hasCameraBounds_ = true;
  cameraBounds_ = Rect{0.0F, 0.0F, kTestCameraW, kTestCameraH};
  cameraDeadzoneW_ = kTestDeadzoneW;
  cameraDeadzoneH_ = kTestDeadzoneH;
  cameraLookaheadX_ = kTestLookaheadX;
  cameraLookaheadY_ = 0.0F;
  cameraLookHoldMs_ = 0;
  cameraLookUpY_ = 0.0F;
  cameraLookDownY_ = 0.0F;
  cameraNoBackscroll_ = false;
  render_ = defaultRenderStyle();

  // floor
  solids_.push_back(SolidRect{Rect{0.0F, 500.0F, 560.0F, 80.0F}, false});
  solids_.push_back(SolidRect{Rect{680.0F, 500.0F, 1320.0F, 80.0F}, false});
  // a small ramp
  slopes_.push_back(SlopeRect{Rect{560.0F, 440.0F, 120.0F, 60.0F}, SlopeDir::UpRight});
  // one-way platform
  solids_.push_back(SolidRect{Rect{260.0F, 340.0F, 220.0F, 20.0F}, true});
  // a block
  solids_.push_back(SolidRect{Rect{420.0F, 420.0F, 120.0F, 24.0F}, false});
  // wall
  solids_.push_back(SolidRect{Rect{740.0F, 360.0F, 30.0F, 220.0F}, false});

  spawns_.emplace("default", SpawnPoint{120.0F, 120.0F, 1});
  spawns_.emplace("left", SpawnPoint{60.0F, 120.0F, 1});
  spawns_.emplace("right", SpawnPoint{1900.0F, 120.0F, -1});
}

bool Stage::loadFromToml(const char* stageTomlPath) {
  toml::table tbl;
  try {
    tbl = toml::parse_file(stageTomlPath);
  } catch (...) {
    return false;
  }

  TomlUtil::warnUnknownKeys(
      tbl, stageTomlPath, "root",
      {"version", "stage", "world", "render", "solids", "slopes", "zones", "hazards", "spawns",
       "enemies", "collectibles", "camera", "collision"});

  std::vector<SolidRect> nextSolids;
  std::vector<SlopeRect> nextSlopes;
  std::vector<StageZone> nextZones;
  std::vector<StageHazard> nextHazards;
  std::vector<CameraLock> nextCameraLocks;
  std::unordered_map<std::string, SpawnPoint> nextSpawns;
  std::vector<EnemySpawn> nextEnemySpawns;
  std::vector<CollectibleSpawn> nextCollectibleSpawns;
  float nextGroundSnap = 2.0F;
  float nextStepUp = 6.0F;
  float nextCollisionSkin = 0.01F;
  bool nextHasWorldBounds = false;
  Rect nextWorldBounds{};
  bool nextHasCameraBounds = false;
  Rect nextCameraBounds{};
  int nextVersion = 0;
  std::string nextId;
  std::string nextDisplay;
  float nextCameraDeadzoneW = 0.0F;
  float nextCameraDeadzoneH = 0.0F;
  float nextCameraLookaheadX = 0.0F;
  float nextCameraLookaheadY = 0.0F;
  int nextCameraLookHoldMs = 0;
  float nextCameraLookUpY = 0.0F;
  float nextCameraLookDownY = 0.0F;
  bool nextCameraNoBackscroll = false;
  StageRenderStyle nextRender = defaultRenderStyle();

  auto readRectFields = [](const toml::table& t, Rect& r) {
    if (auto v = t.get("x"))
      r.x = v->value_or(r.x);
    if (auto v = t.get("y"))
      r.y = v->value_or(r.y);
    if (auto v = t.get("w"))
      r.w = v->value_or(r.w);
    if (auto v = t.get("h"))
      r.h = v->value_or(r.h);
  };

  auto warnOutsideWorldBounds = [&](const Rect& r, std::string_view scope) {
    if (!nextHasWorldBounds)
      return;
    if (r.x < nextWorldBounds.x || r.y < nextWorldBounds.y ||
        r.x + r.w > nextWorldBounds.x + nextWorldBounds.w ||
        r.y + r.h > nextWorldBounds.y + nextWorldBounds.h) {
      TomlUtil::warnf(stageTomlPath, "{} extends outside world bounds; check [world] size", scope);
    }
  };

  if (stageTomlPath && *stageTomlPath) {
    try {
      nextId = std::filesystem::path(stageTomlPath).stem().string();
    } catch (...) {
      nextId.clear();
    }
  }

  if (auto v = tbl["version"].value<int>())
    nextVersion = *v;

  if (auto st = tbl["stage"].as_table()) {
    TomlUtil::warnUnknownKeys(*st, stageTomlPath, "stage", {"id", "display"});
    if (auto v = st->get("id"))
      nextId = v->value_or(nextId);
    if (auto v = st->get("display"))
      nextDisplay = v->value_or(nextDisplay);
  }
  if (nextDisplay.empty())
    nextDisplay = nextId;

  if (auto w = tbl["world"].as_table()) {
    TomlUtil::warnUnknownKeys(*w, stageTomlPath, "world", {"width", "height"});
    float width = 0.0F;
    float height = 0.0F;
    if (auto v = w->get("width"))
      width = v->value_or(width);
    if (auto v = w->get("height"))
      height = v->value_or(height);
    if (width > 0.0F && height > 0.0F) {
      nextHasWorldBounds = true;
      nextWorldBounds = Rect{0.0F, 0.0F, width, height};
    }
  }

  if (auto solids = tbl["solids"].as_array()) {
    std::size_t idx = 0;
    for (const auto& node : *solids) {
      auto t = node.as_table();
      ++idx;
      if (!t)
        continue;

      const std::string scope = "solids[" + std::to_string(idx - 1) + "]";
      TomlUtil::warnUnknownKeys(*t, stageTomlPath, scope, {"x", "y", "w", "h", "one_way"});

      Rect r{};
      readRectFields(*t, r);
      bool oneWay = false;
      if (auto v = t->get("one_way"))
        oneWay = v->value_or(oneWay);

      if (r.w <= 0.0F || r.h <= 0.0F) {
        TomlUtil::warnf(stageTomlPath, "{} has invalid size (w={:.3F} h={:.3F}); skipping",
                        scope.c_str(), r.w, r.h);
        continue;
      }
      warnOutsideWorldBounds(r, scope);
      nextSolids.push_back(SolidRect{r, oneWay});
    }
  }

  if (auto slopes = tbl["slopes"].as_array()) {
    std::size_t idx = 0;
    for (const auto& node : *slopes) {
      auto t = node.as_table();
      ++idx;
      if (!t)
        continue;

      const std::string scope = "slopes[" + std::to_string(idx - 1) + "]";
      TomlUtil::warnUnknownKeys(*t, stageTomlPath, scope, {"x", "y", "w", "h", "dir"});

      Rect r{};
      readRectFields(*t, r);

      std::string dir = "up_right";
      if (auto v = t->get("dir"))
        dir = v->value_or(dir);

      SlopeDir slopeDir = SlopeDir::UpRight;
      if (dir == "up_right" || dir == "right") {
        slopeDir = SlopeDir::UpRight;
      } else if (dir == "up_left" || dir == "left") {
        slopeDir = SlopeDir::UpLeft;
      } else {
        TomlUtil::warnf(stageTomlPath, "{} dir should be 'up_right' or 'up_left' (got '{}')",
                        scope.c_str(), dir.c_str());
      }

      if (r.w <= 0.0F || r.h <= 0.0F) {
        TomlUtil::warnf(stageTomlPath, "{} has invalid size (w={:.3F} h={:.3F}); skipping",
                        scope.c_str(), r.w, r.h);
        continue;
      }
      warnOutsideWorldBounds(r, scope);
      nextSlopes.push_back(SlopeRect{r, slopeDir});
    }
  }

  for (std::size_t i = 0; i < nextSlopes.size(); ++i) {
    for (std::size_t j = i + 1; j < nextSlopes.size(); ++j) {
      const Rect& a = nextSlopes[i].rect;
      const Rect& b = nextSlopes[j].rect;
      if (!aabbOverlap(a.x, a.y, a.w, a.h, b.x, b.y, b.w, b.h))
        continue;
      TomlUtil::warnf(stageTomlPath, "slopes[{}] overlaps slopes[{}]", i, j);
    }
  }

  auto parseZoneType = [&](const std::string& s, const std::string& scope) -> ZoneType {
    if (s == "water")
      return ZoneType::Water;
    if (s == "ice")
      return ZoneType::Ice;
    if (s == "custom" || s.empty())
      return ZoneType::Custom;

    TomlUtil::warnf(stageTomlPath, "{} type should be 'water', 'ice', or 'custom' (got '{}')",
                    scope.c_str(), s.c_str());
    return ZoneType::Custom;
  };

  if (auto zones = tbl["zones"].as_array()) {
    std::size_t idx = 0;
    for (const auto& node : *zones) {
      auto t = node.as_table();
      ++idx;
      if (!t)
        continue;

      const std::string scope = "zones[" + std::to_string(idx - 1) + "]";
      TomlUtil::warnUnknownKeys(
          *t, stageTomlPath, scope,
          {"x", "y", "w", "h", "type", "gravity_multiplier", "max_fall_speed_multiplier",
           "accel_multiplier", "max_speed_multiplier", "friction_multiplier",
           "ground_friction_multiplier", "air_drag_multiplier", "turn_resistance_multiplier",
           "jump_impulse_multiplier"});

      StageZone z{};
      readRectFields(*t, z.rect);

      std::string typeStr = "custom";
      if (auto v = t->get("type"))
        typeStr = v->value_or(typeStr);
      z.type = parseZoneType(typeStr, scope);

      // Type defaults (can be overridden per-zone).
      if (z.type == ZoneType::Water) {
        z.gravityMultiplier = 0.5F;
        z.maxFallSpeedMultiplier = 0.5F;
        z.accelMultiplier = 0.5F;
        z.maxSpeedMultiplier = 0.8F;
        z.frictionMultiplier = 0.5F;
        z.jumpImpulseMultiplier = 0.5F;
      } else if (z.type == ZoneType::Ice) {
        z.frictionMultiplier = 0.15F;
      }

      if (auto v = t->get("gravity_multiplier"))
        z.gravityMultiplier = v->value_or(z.gravityMultiplier);
      if (auto v = t->get("max_fall_speed_multiplier"))
        z.maxFallSpeedMultiplier = v->value_or(z.maxFallSpeedMultiplier);
      if (auto v = t->get("accel_multiplier"))
        z.accelMultiplier = v->value_or(z.accelMultiplier);
      if (auto v = t->get("max_speed_multiplier"))
        z.maxSpeedMultiplier = v->value_or(z.maxSpeedMultiplier);
      if (auto v = t->get("friction_multiplier"))
        z.frictionMultiplier = v->value_or(z.frictionMultiplier);
      if (auto v = t->get("ground_friction_multiplier"))
        z.groundFrictionMultiplier = v->value_or(z.groundFrictionMultiplier);
      if (auto v = t->get("air_drag_multiplier"))
        z.airDragMultiplier = v->value_or(z.airDragMultiplier);
      if (auto v = t->get("turn_resistance_multiplier"))
        z.turnResistanceMultiplier = v->value_or(z.turnResistanceMultiplier);
      if (auto v = t->get("jump_impulse_multiplier"))
        z.jumpImpulseMultiplier = v->value_or(z.jumpImpulseMultiplier);

      if (z.rect.w <= 0.0F || z.rect.h <= 0.0F) {
        TomlUtil::warnf(stageTomlPath, "{} has invalid size (w={:.3F} h={:.3F}); skipping",
                        scope.c_str(), z.rect.w, z.rect.h);
        continue;
      }

      auto clampMul = [&](float& m, const char* key) {
        if (m < 0.0F) {
          TomlUtil::warnf(stageTomlPath, "{} {} must be >= 0 (got {:.3F}); clamping to 0",
                          scope.c_str(), key, m);
          m = 0.0F;
        }
      };
      clampMul(z.gravityMultiplier, "gravity_multiplier");
      clampMul(z.maxFallSpeedMultiplier, "max_fall_speed_multiplier");
      clampMul(z.accelMultiplier, "accel_multiplier");
      clampMul(z.maxSpeedMultiplier, "max_speed_multiplier");
      clampMul(z.frictionMultiplier, "friction_multiplier");
      clampMul(z.groundFrictionMultiplier, "ground_friction_multiplier");
      clampMul(z.airDragMultiplier, "air_drag_multiplier");
      clampMul(z.turnResistanceMultiplier, "turn_resistance_multiplier");
      clampMul(z.jumpImpulseMultiplier, "jump_impulse_multiplier");

      warnOutsideWorldBounds(z.rect, scope);

      nextZones.push_back(z);
    }
  }

  if (auto hazards = tbl["hazards"].as_array()) {
    std::size_t idx = 0;
    for (const auto& node : *hazards) {
      auto t = node.as_table();
      ++idx;
      if (!t)
        continue;

      const std::string scope = "hazards[" + std::to_string(idx - 1) + "]";
      TomlUtil::warnUnknownKeys(*t, stageTomlPath, scope,
                                {"x", "y", "w", "h", "iframes_ms", "ignore_iframes", "lockout_ms",
                                 "knockback_vx", "knockback_vy"});

      StageHazard h{};
      readRectFields(*t, h.rect);

      if (auto v = t->get("iframes_ms"))
        h.iframesMs = v->value_or(h.iframesMs);
      if (auto v = t->get("ignore_iframes"))
        h.ignoreIframes = v->value_or(h.ignoreIframes);
      if (auto v = t->get("lockout_ms"))
        h.lockoutMs = v->value_or(h.lockoutMs);
      if (auto v = t->get("knockback_vx"))
        h.knockbackVx = v->value_or(h.knockbackVx);
      if (auto v = t->get("knockback_vy"))
        h.knockbackVy = v->value_or(h.knockbackVy);

      if (h.rect.w <= 0.0F || h.rect.h <= 0.0F) {
        TomlUtil::warnf(stageTomlPath, "{} has invalid size (w={:.3F} h={:.3F}); skipping",
                        scope.c_str(), h.rect.w, h.rect.h);
        continue;
      }
      if (h.iframesMs < 0) {
        TomlUtil::warnf(stageTomlPath, "{} iframes_ms must be >= 0 (got {}); clamping to 0",
                        scope.c_str(), h.iframesMs);
        h.iframesMs = 0;
      }
      if (h.lockoutMs < 0) {
        TomlUtil::warnf(stageTomlPath, "{} lockout_ms must be >= 0 (got {}); clamping to 0",
                        scope.c_str(), h.lockoutMs);
        h.lockoutMs = 0;
      }

      warnOutsideWorldBounds(h.rect, scope);

      nextHazards.push_back(h);
    }
  }

  if (auto spawns = tbl["spawns"].as_table()) {
    for (const auto& [key, node] : *spawns) {
      auto t = node.as_table();
      if (!t)
        continue;

      const std::string scope = "spawns." + std::string(key.str());
      TomlUtil::warnUnknownKeys(*t, stageTomlPath, scope, {"x", "y", "facing"});

      SpawnPoint sp{};
      if (auto v = t->get("x"))
        sp.x = v->value_or(sp.x);
      if (auto v = t->get("y"))
        sp.y = v->value_or(sp.y);
      if (auto v = t->get("facing")) {
        const int f = v->value_or(sp.facingX);
        if (f < 0)
          sp.facingX = -1;
        else
          sp.facingX = 1;

        if (f != -1 && f != 1) {
          TomlUtil::warnf(stageTomlPath, "spawn '{}' facing should be -1 or 1 (got {})",
                          std::string(key.str()).c_str(), f);
        }
      }
      nextSpawns.emplace(key.str(), sp);
    }
  }

  if (auto enemies = tbl["enemies"].as_array()) {
    std::size_t idx = 0;
    for (const auto& node : *enemies) {
      auto t = node.as_table();
      ++idx;
      if (!t)
        continue;

      const std::string scope = "enemies[" + std::to_string(idx - 1) + "]";
      TomlUtil::warnUnknownKeys(*t, stageTomlPath, scope,
                                {"x", "y", "facing", "patrol_min_x", "patrol_max_x", "config"});

      EnemySpawn enemy{};
      if (auto v = t->get("x"))
        enemy.x = v->value_or(enemy.x);
      if (auto v = t->get("y"))
        enemy.y = v->value_or(enemy.y);
      if (auto v = t->get("facing"))
        enemy.facingX = v->value_or(enemy.facingX);
      if (auto v = t->get("config"))
        enemy.configPath = v->value_or(enemy.configPath);

      if (auto v = t->get("patrol_min_x")) {
        enemy.hasPatrol = true;
        enemy.patrolMinX = v->value_or(enemy.patrolMinX);
      }
      if (auto v = t->get("patrol_max_x")) {
        enemy.hasPatrol = true;
        enemy.patrolMaxX = v->value_or(enemy.patrolMaxX);
      }

      if (enemy.facingX != -1 && enemy.facingX != 1) {
        TomlUtil::warnf(stageTomlPath, "enemy spawn facing should be -1 or 1 (got {}) in {}",
                        enemy.facingX, scope);
        enemy.facingX = (enemy.facingX < 0) ? -1 : 1;
      }
      if (enemy.hasPatrol && enemy.patrolMaxX < enemy.patrolMinX) {
        TomlUtil::warnf(stageTomlPath, "enemy spawn patrol_max_x < patrol_min_x in {}; swapping",
                        scope);
        std::swap(enemy.patrolMinX, enemy.patrolMaxX);
      }

      nextEnemySpawns.push_back(enemy);
    }
  }

  if (auto collectibles = tbl["collectibles"].as_array()) {
    std::size_t idx = 0;
    for (const auto& node : *collectibles) {
      auto t = node.as_table();
      ++idx;
      if (!t)
        continue;

      const std::string scope = "collectibles[" + std::to_string(idx - 1) + "]";
      TomlUtil::warnUnknownKeys(*t, stageTomlPath, scope, {"x", "y", "config"});

      CollectibleSpawn collectible{};
      if (auto v = t->get("x"))
        collectible.x = v->value_or(collectible.x);
      if (auto v = t->get("y"))
        collectible.y = v->value_or(collectible.y);
      if (auto v = t->get("config"))
        collectible.configPath = v->value_or(collectible.configPath);

      nextCollectibleSpawns.push_back(collectible);
    }
  }

  if (auto cam = tbl["camera"].as_table()) {
    TomlUtil::warnUnknownKeys(
        *cam, stageTomlPath, "camera",
        {"deadzone_w", "deadzone_h", "lookahead_x", "lookahead_y", "look_hold_ms", "look_up_y",
         "look_down_y", "no_backscroll", "bounds", "locks"});
    if (auto v = cam->get("deadzone_w"))
      nextCameraDeadzoneW = v->value_or(nextCameraDeadzoneW);
    if (auto v = cam->get("deadzone_h"))
      nextCameraDeadzoneH = v->value_or(nextCameraDeadzoneH);
    if (auto v = cam->get("lookahead_x"))
      nextCameraLookaheadX = v->value_or(nextCameraLookaheadX);
    if (auto v = cam->get("lookahead_y"))
      nextCameraLookaheadY = v->value_or(nextCameraLookaheadY);
    if (auto v = cam->get("look_hold_ms"))
      nextCameraLookHoldMs = v->value_or(nextCameraLookHoldMs);
    if (auto v = cam->get("look_up_y"))
      nextCameraLookUpY = v->value_or(nextCameraLookUpY);
    if (auto v = cam->get("look_down_y"))
      nextCameraLookDownY = v->value_or(nextCameraLookDownY);
    if (auto v = cam->get("no_backscroll"))
      nextCameraNoBackscroll = v->value_or(nextCameraNoBackscroll);

    if (auto b = (*cam)["bounds"].as_table()) {
      TomlUtil::warnUnknownKeys(*b, stageTomlPath, "camera.bounds", {"x", "y", "w", "h"});
      Rect r{};
      readRectFields(*b, r);

      if (r.w > 0.0F && r.h > 0.0F) {
        nextHasCameraBounds = true;
        nextCameraBounds = r;
      }
    }

    if (auto locks = (*cam)["locks"].as_array()) {
      std::size_t idx = 0;
      for (const auto& node : *locks) {
        auto t = node.as_table();
        ++idx;
        if (!t)
          continue;

        const std::string scope = "camera.locks[" + std::to_string(idx - 1) + "]";
        TomlUtil::warnUnknownKeys(
            *t, stageTomlPath, scope,
            {"x", "y", "w", "h", "bounds_x", "bounds_y", "bounds_w", "bounds_h"});

        CameraLock lock{};
        if (auto v = t->get("x"))
          lock.trigger.x = v->value_or(lock.trigger.x);
        if (auto v = t->get("y"))
          lock.trigger.y = v->value_or(lock.trigger.y);
        if (auto v = t->get("w"))
          lock.trigger.w = v->value_or(lock.trigger.w);
        if (auto v = t->get("h"))
          lock.trigger.h = v->value_or(lock.trigger.h);

        if (auto v = t->get("bounds_x"))
          lock.bounds.x = v->value_or(lock.bounds.x);
        if (auto v = t->get("bounds_y"))
          lock.bounds.y = v->value_or(lock.bounds.y);
        if (auto v = t->get("bounds_w"))
          lock.bounds.w = v->value_or(lock.bounds.w);
        if (auto v = t->get("bounds_h"))
          lock.bounds.h = v->value_or(lock.bounds.h);

        if (lock.trigger.w <= 0.0F || lock.trigger.h <= 0.0F || lock.bounds.w <= 0.0F ||
            lock.bounds.h <= 0.0F) {
          TomlUtil::warnf(stageTomlPath, "{} requires positive trigger w/h and bounds_w/bounds_h",
                          scope.c_str());
          continue;
        }

        nextCameraLocks.push_back(lock);
      }
    }
  }

  if (auto collision = tbl["collision"].as_table()) {
    TomlUtil::warnUnknownKeys(*collision, stageTomlPath, "collision",
                              {"ground_snap", "step_up", "skin"});
    if (auto v = collision->get("ground_snap"))
      nextGroundSnap = v->value_or(nextGroundSnap);
    if (auto v = collision->get("step_up"))
      nextStepUp = v->value_or(nextStepUp);
    if (auto v = collision->get("skin"))
      nextCollisionSkin = v->value_or(nextCollisionSkin);
  }

  if (auto render = tbl["render"].as_table()) {
    TomlUtil::warnUnknownKeys(*render, stageTomlPath, "render",
                              {"bg_top", "bg_bottom", "platform_base", "platform_light",
                               "platform_dark", "platform_highlight"});
    maybeReadRenderColor(stageTomlPath, *render, "bg_top", nextRender.bgTop);
    maybeReadRenderColor(stageTomlPath, *render, "bg_bottom", nextRender.bgBottom);
    maybeReadRenderColor(stageTomlPath, *render, "platform_base", nextRender.platformBase);
    maybeReadRenderColor(stageTomlPath, *render, "platform_light", nextRender.platformLight);
    maybeReadRenderColor(stageTomlPath, *render, "platform_dark", nextRender.platformDark);
    maybeReadRenderColor(stageTomlPath, *render, "platform_highlight",
                         nextRender.platformHighlight);
  }

  nextGroundSnap = std::max(0.0F, nextGroundSnap);
  nextStepUp = std::max(0.0F, nextStepUp);
  nextCollisionSkin = std::max(0.0F, nextCollisionSkin);
  nextCameraDeadzoneW = std::max(0.0F, nextCameraDeadzoneW);
  nextCameraDeadzoneH = std::max(0.0F, nextCameraDeadzoneH);
  nextCameraLookaheadX = std::max(0.0F, nextCameraLookaheadX);
  nextCameraLookHoldMs = std::max(0, nextCameraLookHoldMs);
  nextCameraLookUpY = std::max(0.0F, nextCameraLookUpY);
  nextCameraLookDownY = std::max(0.0F, nextCameraLookDownY);

  if (nextSpawns.empty()) {
    nextSpawns.emplace("default", SpawnPoint{120.0F, 120.0F});
  }

  solids_ = std::move(nextSolids);
  slopes_ = std::move(nextSlopes);
  zones_ = std::move(nextZones);
  hazards_ = std::move(nextHazards);
  cameraLocks_ = std::move(nextCameraLocks);
  spawns_ = std::move(nextSpawns);
  enemySpawns_ = std::move(nextEnemySpawns);
  collectibleSpawns_ = std::move(nextCollectibleSpawns);
  version_ = nextVersion;
  id_ = std::move(nextId);
  displayName_ = std::move(nextDisplay);
  groundSnap_ = nextGroundSnap;
  stepUp_ = nextStepUp;
  collisionSkin_ = nextCollisionSkin;
  hasWorldBounds_ = nextHasWorldBounds;
  worldBounds_ = nextWorldBounds;
  hasCameraBounds_ = nextHasCameraBounds;
  cameraBounds_ = nextCameraBounds;
  cameraDeadzoneW_ = nextCameraDeadzoneW;
  cameraDeadzoneH_ = nextCameraDeadzoneH;
  cameraLookaheadX_ = nextCameraLookaheadX;
  cameraLookaheadY_ = nextCameraLookaheadY;
  cameraLookHoldMs_ = nextCameraLookHoldMs;
  cameraLookUpY_ = nextCameraLookUpY;
  cameraLookDownY_ = nextCameraLookDownY;
  cameraNoBackscroll_ = nextCameraNoBackscroll;
  render_ = nextRender;

  return true;
}

bool Stage::getSpawn(const char* name, float& outX, float& outY) const {
  const char* key = (name != nullptr) ? name : "";
  auto it = spawns_.find(key);
  if (it == spawns_.end()) {
    return false;
  }
  outX = it->second.x;
  outY = it->second.y;
  return true;
}

bool Stage::getSpawn(const char* name, float& outX, float& outY, int& outFacingX) const {
  const char* key = (name != nullptr) ? name : "";
  auto it = spawns_.find(key);
  if (it == spawns_.end()) {
    return false;
  }
  outX = it->second.x;
  outY = it->second.y;
  outFacingX = it->second.facingX < 0 ? -1 : 1;
  return true;
}

std::vector<std::string> Stage::spawnNames() const {
  std::vector<std::string> out;
  out.reserve(spawns_.size());
  for (const auto& [name, sp] : spawns_) {
    (void)sp;
    out.push_back(name);
  }
  std::sort(out.begin(), out.end());
  return out;
}

bool Stage::getWorldBounds(Rect& out) const {
  if (!hasWorldBounds_)
    return false;
  out = worldBounds_;
  return true;
}

bool Stage::getCameraBounds(Rect& out) const {
  if (!hasCameraBounds_)
    return false;
  out = cameraBounds_;
  return true;
}

bool Stage::cameraLockBoundsAt(const Rect& aabb, Rect& outBounds) const {
  for (const auto& lock : cameraLocks_) {
    const Rect& r = lock.trigger;
    if (!aabbOverlap(aabb.x, aabb.y, aabb.w, aabb.h, r.x, r.y, r.w, r.h))
      continue;
    outBounds = lock.bounds;
    return true;
  }
  return false;
}

StageEnvironmentMultipliers Stage::environmentAt(const Rect& aabb) const {
  StageEnvironmentMultipliers out{};
  for (const auto& z : zones_) {
    const Rect& r = z.rect;
    if (!aabbOverlap(aabb.x, aabb.y, aabb.w, aabb.h, r.x, r.y, r.w, r.h))
      continue;

    if (z.type == ZoneType::Water)
      out.inWater = true;
    if (z.type == ZoneType::Ice)
      out.inIce = true;

    out.gravity *= z.gravityMultiplier;
    out.maxFallSpeed *= z.maxFallSpeedMultiplier;
    out.accel *= z.accelMultiplier;
    out.maxSpeed *= z.maxSpeedMultiplier;
    out.friction *= z.frictionMultiplier;
    out.groundFriction *= z.groundFrictionMultiplier;
    out.airDrag *= z.airDragMultiplier;
    out.turnResistance *= z.turnResistanceMultiplier;
    out.jumpImpulse *= z.jumpImpulseMultiplier;
  }
  return out;
}

bool Stage::hazardAt(const Rect& aabb, StageHazard& out) const {
  for (const auto& h : hazards_) {
    const Rect& r = h.rect;
    if (!aabbOverlap(aabb.x, aabb.y, aabb.w, aabb.h, r.x, r.y, r.w, r.h))
      continue;
    out = h;
    return true;
  }
  return false;
}

bool Stage::overlapsSolid(const Rect& aabb, bool ignoreOneWay) const {
  return std::ranges::any_of(solids_, [&](const auto& solid) {
    if (ignoreOneWay && solid.oneWay) {
      return false;
    }
    const Rect& r = solid.rect;
    return aabbOverlap(aabb.x, aabb.y, aabb.w, aabb.h, r.x, r.y, r.w, r.h);
  });
}

bool Stage::touchingWall(const Rect& aabb, int dirX, float probe) const {
  const int d = (dirX < 0) ? -1 : 1;
  if (dirX == 0 || probe <= 0.0F) {
    return false;
  }

  const float skinY = std::min(collisionSkin_, aabb.h * 0.49F);
  const float overlapY = aabb.h - skinY * 2.0F;

  const float testX = aabb.x + static_cast<float>(d) * probe;
  const float testY = aabb.y + skinY;

  return std::ranges::any_of(solids_, [&](const auto& solid) {
    if (solid.oneWay) {
      return false;
    }
    const Rect& r = solid.rect;
    return aabbOverlap(testX, testY, aabb.w, overlapY, r.x, r.y, r.w, r.h);
  });
}

bool Stage::groundIsOneWay(const Rect& aabb) const {
  const float skinX = std::min(collisionSkin_, aabb.w * 0.49F);
  const float overlapX = aabb.w - skinX * 2.0F;
  if (overlapX <= 0.0F) {
    return false;
  }

  const float bottom = aabb.y + aabb.h;
  const float tolerance = std::max(1.0F, groundSnap_ + 0.5F);

  return std::ranges::any_of(solids_, [&](const auto& solid) {
    if (!solid.oneWay) {
      return false;
    }
    const Rect& r = solid.rect;
    if (aabb.x + skinX >= r.x + r.w) {
      return false;
    }
    if (aabb.x + skinX + overlapX <= r.x) {
      return false;
    }

    const float top = r.y;
    return (bottom >= top - tolerance) && (bottom <= top + tolerance);
  });
}

EntityId Stage::spawnDemo(World& w,
                          const char* characterTomlPath,
                          float spawnX,
                          float spawnY,
                          int facingX) {
  CharacterConfig cfg;
  if (!cfg.loadFromToml(characterTomlPath)) {
    std::cerr << "Failed to load character TOML: " << characterTomlPath << '\n';
  }

  EntityId e = w.create();
  w.player = e;

  w.registry.emplace<Transform>(e, Vec2{spawnX, spawnY});
  w.registry.emplace<Velocity>(e, Vec2{0.0F, 0.0F});
  w.registry.emplace<AABB>(e, cfg.collision.w, cfg.collision.h);
  w.registry.emplace<InputState>(e);
  w.registry.emplace<Grounded>(e, false);
  w.registry.emplace<DebugName>(e, cfg.displayName);
  w.registry.emplace<AnimState>(e);
  auto& as = w.registry.emplace<ActionState>(e);
  as.facingX = (facingX < 0) ? -1 : 1;
  w.registry.emplace<CharacterTag>(e);

  // Timer components for display in debug UI
  w.registry.emplace<CoyoteFrames>(e, 0);
  w.registry.emplace<JumpBufferFrames>(e, 0);
  w.registry.emplace<ActionCooldownFrames>(e, 0);
  w.registry.emplace<AttackCooldownFrames>(e, 0);
  w.registry.emplace<DropThroughFrames>(e, 0);
  w.registry.emplace<HurtLockoutFrames>(e, 0);

  // attach config + controller
  CharacterController::attach(e, cfg);
  CharacterController::setFacing(e, facingX);

  return e;
}

void Stage::spawnEnemies(World& w) const {
  for (const auto& spawn : enemySpawns_) {
    if (spawn.configPath.empty()) {
      std::cerr << "Enemy spawn missing config path\n";
      continue;
    }

    const std::string configPath = Paths::resolveAssetPath(spawn.configPath);
    EnemyConfig cfg;
    if (!cfg.loadFromToml(configPath.c_str())) {
      std::cerr << "Failed to load enemy TOML: " << configPath << '\n';
      continue;
    }

    EntityId e = w.create();
    w.registry.emplace<Transform>(e, Vec2{spawn.x, spawn.y});
    w.registry.emplace<Velocity>(e, Vec2{0.0F, 0.0F});
    w.registry.emplace<AABB>(e, cfg.collision.w, cfg.collision.h);
    w.registry.emplace<Grounded>(e, false);
    w.registry.emplace<DebugName>(e, cfg.displayName);
    w.registry.emplace<EnemyTag>(e);
    w.registry.emplace<EnemyConfig>(e, cfg);

    EnemyState state{};
    state.facingX = (spawn.facingX < 0) ? -1 : 1;
    state.hasPatrol = spawn.hasPatrol;
    state.patrolMinX = spawn.patrolMinX;
    state.patrolMaxX = spawn.patrolMaxX;
    state.health = cfg.combat.health;
    if (cfg.type == EnemyConfig::Type::Shooter) {
      state.attackTimerFrames = std::max(0, cfg.shooter.warmupFrames);
    }
    w.registry.emplace<EnemyState>(e, state);
  }
}

void Stage::spawnCollectibles(World& w) const {
  for (const auto& spawn : collectibleSpawns_) {
    if (spawn.configPath.empty()) {
      std::cerr << "Collectible spawn missing config path\n";
      continue;
    }

    const std::string configPath = Paths::resolveAssetPath(spawn.configPath);
    CollectibleConfig cfg;
    if (!cfg.loadFromToml(configPath.c_str())) {
      std::cerr << "Failed to load collectible TOML: " << configPath << '\n';
      continue;
    }

    EntityId e = w.create();
    w.registry.emplace<Transform>(e, Vec2{spawn.x, spawn.y});
    w.registry.emplace<AABB>(e, cfg.collision.w, cfg.collision.h);
    w.registry.emplace<DebugName>(e, cfg.displayName);
    w.registry.emplace<CollectibleTag>(e);
    w.registry.emplace<CollectibleConfig>(e, cfg);

    CollectibleState state{};
    state.value = cfg.value.score;
    state.healthRestore = cfg.value.health;
    w.registry.emplace<CollectibleState>(e, state);
  }
}

// NOLINTNEXTLINE
bool Stage::resolveAABBCollision(Transform& t,
                                 Velocity& v,
                                 const AABB& box,
                                 Grounded& g,
                                 bool ignoreOneWay,
                                 bool snapCollisionToPixel) {
  auto overlap = [&](float ax, float ay, float aw, float ah, float bx, float by, float bw,
                     float bh) -> bool {
    if (snapCollisionToPixel) {
      ax = std::floor(ax);
      ay = std::floor(ay);
    }
    return aabbOverlap(ax, ay, aw, ah, bx, by, bw, bh);
  };

  const bool wasGrounded = g.onGround;
  g.onGround = false;

  // Resolve per-axis to reduce sticky corners and grounding flicker:
  // 1) integrate + resolve X
  // 2) integrate + resolve Y (grounding comes from downward contacts)

  const float oldX = t.pos.x;
  const float dx = v.v.x;  // velocity is in px/frame
  t.pos.x += dx;

  const float skinY = std::min(collisionSkin_, box.h * 0.49F);
  const float overlapY = box.h - skinY * 2.0F;

  for (const auto& solid : solids_) {
    if (solid.oneWay) {
      continue;  // one-way platforms don't collide on X
    }

    const Rect& s = solid.rect;
    if (!overlap(t.pos.x, t.pos.y + skinY, box.w, overlapY, s.x, s.y, s.w, s.h)) {
      continue;
    }

    // Step-up: allow a small vertical correction when bumping into a low obstacle while grounded.
    if (stepUp_ > 0.0F && wasGrounded && dx != 0.0F && v.v.y >= 0.0F) {
      const float targetY = s.y - box.h;
      const float step = t.pos.y - targetY;  // positive = move up
      if (step > 0.0F && step <= stepUp_) {
        bool clear = true;
        for (const auto& other : solids_) {
          if (other.oneWay) {
            continue;
          }
          const Rect& o = other.rect;
          if (overlap(t.pos.x, targetY, box.w, box.h, o.x, o.y, o.w, o.h)) {
            clear = false;
            break;
          }
        }
        if (clear) {
          t.pos.y = targetY;
          continue;
        }
      }
    }

    const bool cameFromLeft = (dx > 0.0F) || (dx == 0.0F && (oldX + box.w) <= s.x);
    const bool cameFromRight = (dx < 0.0F) || (dx == 0.0F && oldX >= (s.x + s.w));

    if (cameFromLeft) {
      t.pos.x = s.x - box.w;
    } else if (cameFromRight) {
      t.pos.x = s.x + s.w;
    } else {
      const float leftPen = (t.pos.x + box.w) - s.x;
      const float rightPen = (s.x + s.w) - t.pos.x;
      if (leftPen < rightPen) {
        t.pos.x -= leftPen;
      } else {
        t.pos.x += rightPen;
      }
    }

    v.v.x = 0.0F;
  }

  // Slope side collision (minimal): treat the vertical edge of the triangle as a wall
  // only when entering the slope volume from that "vertical" side.
  //
  // Slopes are otherwise one-way (walkable from above), and we allow exiting the slope at
  // the top edge (e.g. onto an adjacent platform).
  for (const auto& slope : slopes_) {
    const Rect& r = slope.rect;
    if (r.w <= 0.0F || r.h <= 0.0F) {
      continue;
    }

    const float ay0 = t.pos.y + skinY;
    const float ay1 = ay0 + overlapY;
    const float by0 = r.y;
    const float by1 = r.y + r.h;
    if (ay0 >= by1 || ay1 <= by0) {
      continue;
    }

    const float edgeX = (slope.dir == SlopeDir::UpRight) ? (r.x + r.w) : r.x;

    // UpRight: vertical edge is on the right; block entering from the right.
    if (slope.dir == SlopeDir::UpRight) {
      if (dx < 0.0F && oldX >= edgeX && t.pos.x < edgeX) {
        t.pos.x = edgeX;
        v.v.x = 0.0F;
      }
      continue;
    }

    // UpLeft: vertical edge is on the left; block entering from the left.
    if (dx > 0.0F && oldX + box.w <= edgeX && t.pos.x + box.w > edgeX) {
      t.pos.x = edgeX - box.w;
      v.v.x = 0.0F;
    }
  }

  const float oldY = t.pos.y;
  const float dy = v.v.y;  // velocity is in px/frame
  t.pos.y += dy;

  const float skinX = std::min(collisionSkin_, box.w * 0.49F);
  const float overlapX = box.w - skinX * 2.0F;

  for (const auto& s : solids_) {
    const Rect& rect = s.rect;
    if (!overlap(t.pos.x + skinX, t.pos.y, overlapX, box.h, rect.x, rect.y, rect.w, rect.h)) {
      continue;
    }

    if (s.oneWay) {
      if (ignoreOneWay) {
        continue;
      }
      if (dy < 0.0F) {
        continue;
      }

      const float oldBottom = oldY + box.h;
      const float top = rect.y;
      if (oldBottom > top + groundSnap_) {
        continue;  // came from below / already below surface
      }

      t.pos.y = rect.y - box.h;
      g.onGround = true;
      v.v.y = 0.0F;
      continue;
    }

    const bool cameFromAbove = (dy > 0.0F) || (dy == 0.0F && (oldY + box.h) <= rect.y);
    const bool cameFromBelow = (dy < 0.0F) || (dy == 0.0F && oldY >= (rect.y + rect.h));

    if (cameFromAbove) {
      t.pos.y = rect.y - box.h;
      g.onGround = true;
    } else if (cameFromBelow) {
      t.pos.y = rect.y + rect.h;
    } else {
      const float topPen = (t.pos.y + box.h) - rect.y;
      const float bottomPen = (rect.y + rect.h) - t.pos.y;
      if (topPen < bottomPen) {
        t.pos.y -= topPen;
        g.onGround = true;
      } else {
        t.pos.y += bottomPen;
      }
    }

    v.v.y = 0.0F;
  }

  // Support snap: if we're within a small tolerance of a walkable surface (solid top or slope),
  // snap onto the best candidate. This avoids seam flicker and helps keep grounding stable.
  if (v.v.y >= 0.0F) {
    struct SupportCandidate {
      float y = 0.0F;
      bool found = false;
      bool isSlope = false;
    };

    static constexpr float kTieEpsilon =
        0.01F;  // treat tiny differences as ties (stability > microns)
    SupportCandidate best{t.pos.y, false, false};

    auto consider = [&](float candidateY, bool isSlope) {
      if (!best.found) {
        best = SupportCandidate{candidateY, true, isSlope};
        return;
      }

      const float diff = candidateY - best.y;
      if (diff < -kTieEpsilon) {
        best = SupportCandidate{candidateY, true, isSlope};
        return;
      }
      if (std::fabs(diff) <= kTieEpsilon) {
        // Prefer flat solids over slopes when effectively tied (reduces seam jitter).
        if (!isSlope && best.isSlope) {
          best = SupportCandidate{candidateY, true, false};
        }
      }
    };

    const float oldBottom = oldY + box.h;
    const float bottom = t.pos.y + box.h;

    // Slope ground resolution (minimal): treat slopes as walkable surfaces from above only.
    const float baseSnap = std::max(groundSnap_, stepUp_);
    if (baseSnap > 0.0F && !slopes_.empty()) {
      static constexpr float kSlopeFootInset = 2.0F;
      const float inset = std::min(kSlopeFootInset, box.w * 0.49F);
      const std::array<float, 3> footOffsets{inset, box.w * 0.5F, box.w - inset};
      const float movedX = std::fabs(t.pos.x - oldX);
      float xMargin = std::max(kSlopeFootInset, collisionSkin_);
      if (wasGrounded && movedX > 0.0F) {
        // When walking from flat ground onto a slope, allow sampling slightly outside the slope's
        // X-range. This enables small “step up onto slope” transitions without requiring a solid
        // ramp volume.
        xMargin = std::max(xMargin, baseSnap);
      }

      for (const auto& slope : slopes_) {
        const Rect& r = slope.rect;
        float snap = baseSnap;
        if (wasGrounded && r.w > 0.0F) {
          // When we were grounded last frame, allow a larger snap distance proportional to how far
          // we moved in X. This keeps us “stuck” to slopes at higher speeds (otherwise we can
          // briefly become airborne and jitter/fall through).
          const float slopeRatio = r.h / r.w;  // |dy| per dx along the slope surface
          snap = std::max(snap, baseSnap + movedX * slopeRatio);
        }

        for (const float off : footOffsets) {
          const float footX = t.pos.x + off;
          if (footX < r.x - xMargin || footX > r.x + r.w + xMargin) {
            continue;
          }

          const float sampleX = std::clamp(footX, r.x, r.x + r.w);
          const float surface = slopeSurfaceY(slope, sampleX);
          const float oldFootX = oldX + off;
          const float prevX = std::clamp(oldFootX, r.x, r.x + r.w);
          const float prevSurface = slopeSurfaceY(slope, prevX);
          if (oldBottom > prevSurface + baseSnap) {
            continue;  // came from below / already below surface
          }

          const float gap = surface - bottom;
          if (gap > snap) {
            continue;  // too far above to snap
          }

          if (gap < 0.0F) {
            // Step up onto slopes only when walking (and within the configured step-up distance).
            if (!wasGrounded || stepUp_ <= 0.0F || (-gap) > stepUp_) {
              continue;
            }
          }

          consider(surface - box.h, true);
        }
      }
    }

    // Ground snap: if we're within a small tolerance above a solid top, snap down.
    if (groundSnap_ > 0.0F) {
      for (const auto& solid : solids_) {
        if (ignoreOneWay && solid.oneWay) {
          continue;
        }
        const Rect& s = solid.rect;
        if (t.pos.x + box.w <= s.x) {
          continue;
        }
        if (t.pos.x >= s.x + s.w) {
          continue;
        }

        const float top = s.y;
        const float penetrationTol = snapCollisionToPixel ? 1.0F : 0.0F;
        if (bottom > top + penetrationTol) {
          continue;
        }

        const float gap = top - bottom;
        if (gap < -penetrationTol || gap > groundSnap_) {
          continue;
        }

        consider(top - box.h, false);
      }
    }

    if (best.found) {
      t.pos.y = best.y;
      v.v.y = 0.0F;
      g.onGround = true;
    }
  }

  return g.onGround;
}

void Stage::renderBackground(Visual::ShapeRenderer& shapes, int viewW, int viewH) const {
  if (viewW <= 0 || viewH <= 0) {
    return;
  }
  const Visual::Color top{render_.bgTop.r, render_.bgTop.g, render_.bgTop.b, render_.bgTop.a};
  const Visual::Color bottom{render_.bgBottom.r, render_.bgBottom.g, render_.bgBottom.b,
                             render_.bgBottom.a};
  shapes.fillGradientV(0.0F, 0.0F, static_cast<float>(viewW), static_cast<float>(viewH), top,
                       bottom);
}

void Stage::render(SDL_Renderer& r,
                   Visual::ShapeRenderer& shapes,
                   const World& w,
                   float camX,
                   float camY) const {
  (void)r;

  const Visual::Color platformBase{render_.platformBase.r, render_.platformBase.g,
                                   render_.platformBase.b, render_.platformBase.a};
  const Visual::Color platformLight{render_.platformLight.r, render_.platformLight.g,
                                    render_.platformLight.b, render_.platformLight.a};
  const Visual::Color platformDark{render_.platformDark.r, render_.platformDark.g,
                                   render_.platformDark.b, render_.platformDark.a};
  const Visual::Color platformHighlight{render_.platformHighlight.r, render_.platformHighlight.g,
                                        render_.platformHighlight.b, render_.platformHighlight.a};

  static constexpr float kBevelSize = 3.0F;

  for (const auto& solid : solids_) {
    const Rect& s = solid.rect;
    float x = s.x - camX;
    float y = s.y - camY;
    float w = s.w;
    float h = s.h;

    // Skip tiny platforms
    if (w < 1.0F || h < 1.0F) {
      continue;
    }

    // One-way platforms are thinner with different style
    if (solid.oneWay) {
      // Thin platform with highlight top
      shapes.fillRect(x, y, w, h, platformBase, 2.0F);
      // Top highlight
      shapes.fillRect(x, y, w, 2.0F, platformHighlight, 1.0F);
      continue;
    }

    // Full bevel rendering for solid platforms
    float bevel = std::min(kBevelSize, std::min(w, h) * 0.25F);

    // Main body
    shapes.fillRect(x, y, w, h, platformBase, 0);

    // Top edge - highlight (lit from above-left)
    shapes.fillRect(x, y, w, bevel, platformLight, 0);

    // Left edge - partial highlight
    Visual::Color leftColor{(uint8_t)((platformBase.r + platformLight.r) / 2),
                            (uint8_t)((platformBase.g + platformLight.g) / 2),
                            (uint8_t)((platformBase.b + platformLight.b) / 2)};
    shapes.fillRect(x, y + bevel, bevel, h - bevel * 2, leftColor, 0);

    // Right edge - partial shadow
    Visual::Color rightColor{(uint8_t)((platformBase.r + platformDark.r) / 2),
                             (uint8_t)((platformBase.g + platformDark.g) / 2),
                             (uint8_t)((platformBase.b + platformDark.b) / 2)};
    shapes.fillRect(x + w - bevel, y + bevel, bevel, h - bevel * 2, rightColor, 0);

    // Bottom edge - shadow
    shapes.fillRect(x, y + h - bevel, w, bevel, platformDark, 0);

    // Top-left corner highlight
    shapes.fillRect(x, y, bevel, bevel, platformHighlight, 0);
  }

  for (const auto& slope : slopes_) {
    const Rect& s = slope.rect;
    float x = s.x - camX;
    float y = s.y - camY;
    float w = s.w;
    float h = s.h;

    if (w < 1.0F || h < 1.0F) {
      continue;
    }

    if (slope.dir == SlopeDir::UpRight) {
      shapes.fillTriangle(Visual::Vec2{x, y + h}, Visual::Vec2{x + w, y + h},
                          Visual::Vec2{x + w, y}, platformBase);
    } else {
      shapes.fillTriangle(Visual::Vec2{x, y}, Visual::Vec2{x, y + h}, Visual::Vec2{x + w, y + h},
                          platformBase);
    }
  }

  (void)w;
}

// NOLINTNEXTLINE
void Stage::renderDebugCollision(SDL_Renderer& r, const World& w, float camX, float camY) const {
  bool hasOneWay = false;
  for (const auto& solid : solids_) {
    const bool isOneWay = solid.oneWay;
    if (isOneWay) {
      hasOneWay = true;
    }
    if (isOneWay) {
      SDL_SetRenderDrawColor(&r, kDebugOneWay.r, kDebugOneWay.g, kDebugOneWay.b, kDebugOneWay.a);
    } else {
      SDL_SetRenderDrawColor(&r, kDebugSolid.r, kDebugSolid.g, kDebugSolid.b, kDebugSolid.a);
    }

    const Rect& s = solid.rect;
    SDL_FRect fr{s.x - camX, s.y - camY, s.w, s.h};
    SDL_RenderRect(&r, &fr);
  }

  if (hasOneWay) {
    SDL_SetRenderDrawColor(&r, kDebugOneWay.r, kDebugOneWay.g, kDebugOneWay.b, kDebugOneWay.a);
    SDL_RenderDebugText(&r, 8.0F, 16.0F, "yellow: one-way");
  }

  if (!slopes_.empty()) {
    SDL_SetRenderDrawColor(&r, kDebugSlope.r, kDebugSlope.g, kDebugSlope.b, kDebugSlope.a);
    for (const auto& slope : slopes_) {
      const Rect& s = slope.rect;
      SDL_FRect fr{s.x - camX, s.y - camY, s.w, s.h};
      SDL_RenderRect(&r, &fr);

      const float x0 = s.x;
      const float x1 = s.x + s.w;
      const bool upRight = (slope.dir == SlopeDir::UpRight);
      const float y0 = upRight ? (s.y + s.h) : s.y;
      const float y1 = upRight ? s.y : (s.y + s.h);
      (void)SDL_RenderLine(&r, x0 - camX, y0 - camY, x1 - camX, y1 - camY);
    }
    SDL_RenderDebugText(&r, 8.0F, 52.0F, "aqua: slopes");
  }

  if (hasWorldBounds_) {
    SDL_SetRenderDrawColor(&r, kDebugWorld.r, kDebugWorld.g, kDebugWorld.b, kDebugWorld.a);
    SDL_FRect fr{worldBounds_.x - camX, worldBounds_.y - camY, worldBounds_.w, worldBounds_.h};
    SDL_RenderRect(&r, &fr);
    SDL_RenderDebugText(&r, 8.0F, 28.0F, "blue: world bounds");
  }

  if (hasCameraBounds_) {
    SDL_SetRenderDrawColor(&r, kDebugCamera.r, kDebugCamera.g, kDebugCamera.b, kDebugCamera.a);
    SDL_FRect fr{cameraBounds_.x - camX, cameraBounds_.y - camY, cameraBounds_.w, cameraBounds_.h};
    SDL_RenderRect(&r, &fr);
    SDL_RenderDebugText(&r, 8.0F, 40.0F, "purple: camera bounds");
  }

  if (!zones_.empty()) {
    bool hasWater = false;
    bool hasIce = false;
    for (const auto& z : zones_) {
      if (z.type == ZoneType::Water) {
        SDL_SetRenderDrawColor(&r, kDebugWater.r, kDebugWater.g, kDebugWater.b, kDebugWater.a);
        hasWater = true;
      } else if (z.type == ZoneType::Ice) {
        SDL_SetRenderDrawColor(&r, kDebugIce.r, kDebugIce.g, kDebugIce.b, kDebugIce.a);
        hasIce = true;
      } else {
        SDL_SetRenderDrawColor(&r, kDebugZone.r, kDebugZone.g, kDebugZone.b, kDebugZone.a);
      }

      const Rect& zr = z.rect;
      SDL_FRect fr{zr.x - camX, zr.y - camY, zr.w, zr.h};
      SDL_RenderRect(&r, &fr);
    }

    if (hasWater) {
      SDL_RenderDebugText(&r, 8.0F, 64.0F, "light blue: water zones");
    }
    if (hasIce) {
      SDL_RenderDebugText(&r, 8.0F, 76.0F, "pale blue: ice zones");
    }
  }

  if (!hazards_.empty()) {
    SDL_SetRenderDrawColor(&r, kDebugHazard.r, kDebugHazard.g, kDebugHazard.b, kDebugHazard.a);
    for (const auto& h : hazards_) {
      const Rect& hr = h.rect;
      SDL_FRect fr{hr.x - camX, hr.y - camY, hr.w, hr.h};
      SDL_RenderRect(&r, &fr);
    }
    SDL_RenderDebugText(&r, 8.0F, 88.0F, "red: hazards");
  }

  if (!spawns_.empty()) {
    SDL_SetRenderDrawColor(&r, kDebugSpawn.r, kDebugSpawn.g, kDebugSpawn.b, kDebugSpawn.a);
    for (const auto& [name, sp] : spawns_) {
      const float sx = sp.x - camX;
      const float sy = sp.y - camY;
      SDL_FRect dot{sx - 2.0F, sy - 2.0F, 4.0F, 4.0F};
      SDL_RenderFillRect(&r, &dot);
      SDL_RenderRect(&r, &dot);

      const float dir = (sp.facingX < 0) ? -1.0F : 1.0F;
      static constexpr float kArrowLen = 14.0F;
      static constexpr float kHead = 4.0F;
      static constexpr float kHeadH = 3.0F;

      const float ex = sx + dir * kArrowLen;
      const float ey = sy;
      (void)SDL_RenderLine(&r, sx, sy, ex, ey);
      (void)SDL_RenderLine(&r, ex, ey, ex - dir * kHead, ey - kHeadH);
      (void)SDL_RenderLine(&r, ex, ey, ex - dir * kHead, ey + kHeadH);

      SDL_RenderDebugText(&r, sx + 6.0F, sy - 6.0F, name.c_str());
    }
  }

  auto view = w.registry.view<Transform, AABB>();
  for (auto entity : view) {
    const auto& t = view.get<Transform>(entity);
    const auto& box = view.get<AABB>(entity);

    bool grounded = false;
    if (auto* g = w.registry.try_get<Grounded>(entity)) {
      grounded = g->onGround;
    }

    if (grounded) {
      SDL_SetRenderDrawColor(&r, 0, 255, 0, 255);
    } else {
      SDL_SetRenderDrawColor(&r, 255, 64, 64, 255);
    }

    SDL_FRect fr{t.pos.x - camX, t.pos.y - camY, box.w, box.h};
    SDL_RenderRect(&r, &fr);
  }
}
