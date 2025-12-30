#include "core/App.h"
#include "core/InputScript.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <SDL3/SDL_hints.h>

#include "character/Actions.h"
#include "character/CharacterConfig.h"
#include "enemy/EnemyConfig.h"
#include "stage/Stage.h"
#include "util/Paths.h"
#include "util/TomlUtil.h"
#include "visual/Form.h"
#include "visual/FormToml.h"

namespace {

static constexpr const char* kDefaultStageToml = "assets/stages/skyway_run.toml";
static constexpr const char* kDefaultCharacterToml = "assets/characters/bolt.toml";

}  // namespace

static void usage(const char* argv0) {
  std::printf(
      "usage: %s [--frames N] [--video-driver NAME] [--stage PATH|--stage-id ID] [--character "
      "PATH|--character-id ID] [--spawn-point NAME] [--spawn X Y] [--no-ui] [--no-render] "
      "[--no-prefs] [--input-script PATH] [--expect-grounded] [--expect-no-respawn] "
      "[--expect-min-x X] [--expect-max-x X] [--expect-min-vx X] [--expect-max-vx X] "
      "[--expect-min-y Y] [--expect-max-y Y] [--expect-min-cam-x X] [--expect-max-cam-x X] "
      "[--expect-min-cam-y Y] [--expect-max-cam-y Y] [--expect-no-camera-backscroll] "
      "[--expect-hurt-count N] [--expect-enemy-kills N] "
      "[--validate [--strict]]\n",
      argv0);
  std::printf("  --frames N           Run N frames then exit (smoke test)\n");
  std::printf("  --video-driver NAME  Force SDL video backend (e.g. x11, wayland, offscreen)\n");
  std::printf("  --stage PATH         Stage TOML (default: %s)\n", kDefaultStageToml);
  std::printf("  --stage-id ID        Stage id (resolves in assets/stages)\n");
  std::printf("  --character PATH     Character TOML (default: %s)\n", kDefaultCharacterToml);
  std::printf("  --character-id ID    Character id (resolves in assets/characters)\n");
  std::printf("  --spawn-point NAME   Named stage spawn point (overrides --spawn)\n");
  std::printf("  --spawn X Y          Spawn position in pixels (default: 120 120)\n");
  std::printf("  --no-prefs           Disable load/save of session prefs (ignore session.toml)\n");
  std::printf("  --input-script PATH  Drive gameplay input from a TOML script (deterministic)\n");
  std::printf("  --no-ui              Disable ImGui panels (still renders)\n");
  std::printf("  --no-render          Run without creating a window/renderer (headless)\n");
  std::printf(
      "  --expect-grounded    Fail if player is not grounded at exit (use with --frames)\n");
  std::printf("  --expect-no-respawn  Fail if player respawned during run (use with --frames)\n");
  std::printf(
      "  --expect-min-x X     Fail if player x at exit is less than X (use with --frames)\n");
  std::printf(
      "  --expect-max-x X     Fail if player x at exit is greater than X (use with --frames)\n");
  std::printf(
      "  --expect-min-vx X    Fail if player vx at exit is less than X (use with --frames)\n");
  std::printf(
      "  --expect-max-vx X    Fail if player vx at exit is greater than X (use with --frames)\n");
  std::printf(
      "  --expect-min-y Y     Fail if player y at exit is less than Y (use with --frames)\n");
  std::printf(
      "  --expect-max-y Y     Fail if player y at exit is greater than Y (use with --frames)\n");
  std::printf(
      "  --expect-min-cam-x X Fail if camera x at exit is less than X (use with --frames)\n");
  std::printf(
      "  --expect-max-cam-x X Fail if camera x at exit is greater than X (use with --frames)\n");
  std::printf(
      "  --expect-min-cam-y Y Fail if camera y at exit is less than Y (use with --frames)\n");
  std::printf(
      "  --expect-max-cam-y Y Fail if camera y at exit is greater than Y (use with --frames)\n");
  std::printf(
      "  --expect-no-camera-backscroll  Fail if camera x decreases during run (use with "
      "--frames)\n");
  std::printf("  --expect-hurt-count N Fail if hurt events during run != N (use with --frames)\n");
  std::printf("  --expect-enemy-kills N Fail if enemy kills during run != N (use with --frames)\n");
  std::printf("  --list-stages        List stage TOML paths\n");
  std::printf("  --list-stage-ids     List stages as: id <tab> display <tab> path\n");
  std::printf("  --list-characters    List character TOML paths\n");
  std::printf("  --list-character-ids List characters as: id <tab> display <tab> path\n");
  std::printf("  --list-spawns        List spawns as: name <tab> x <tab> y <tab> facing\n");
  std::printf(
      "  --validate           Validate stage/character/enemy/form/input script TOML (uses --stage/"
      "--character/--input-script if provided; "
      "otherwise validates all assets)\n");
  std::printf(
      "  --strict             With --validate, treat warnings as errors (includes TOML parse "
      "warnings)\n");
}

static bool parseFloat(const char* s, float& out) {
  char* end = nullptr;
  out = std::strtof(s, &end);
  return end && *end == '\0';
}

static bool parseInt(const char* s, int& out) {
  char* end = nullptr;
  const long v = std::strtol(s, &end, 10);
  if (!end || *end != '\0')
    return false;
  if (v < static_cast<long>(std::numeric_limits<int>::min()) ||
      v > static_cast<long>(std::numeric_limits<int>::max())) {
    return false;
  }
  out = static_cast<int>(v);
  return true;
}

struct ValidateStats {
  int stageFiles = 0;
  int characterFiles = 0;
  int enemyFiles = 0;
  int formFiles = 0;
  int inputScriptFiles = 0;
  int errors = 0;
  int warnings = 0;
};

struct StageMeta {
  std::string id;
  std::string display;
};

struct CharacterMeta {
  std::string id;
  std::string display;
};

struct EnemyMeta {
  std::string id;
  std::string display;
};

struct FormMeta {
  std::string id;
};

static float absf(float v) {
  return (v < 0.0F) ? -v : v;
}

static bool nearlyEqual(float a, float b, float eps = 0.001F) {
  return absf(a - b) <= eps;
}

static int normalizeFacing(int facing) {
  return (facing < 0) ? -1 : 1;
}

static void validateVReport(ValidateStats& st,
                            bool isError,
                            const std::string& path,
                            const char* fmt,
                            std::va_list args) {
  if (isError)
    ++st.errors;
  else
    ++st.warnings;

  const char* kind = isError ? "error" : "warning";
  std::fprintf(stderr, "%s: %s: ", path.c_str(), kind);
  // NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized) - args initialized by caller
  std::vfprintf(stderr, fmt, args);
  std::fprintf(stderr, "\n");
}

static void validateError(ValidateStats& st, const std::string& path, const char* fmt, ...) {
  std::va_list args;
  va_start(args, fmt);
  validateVReport(st, true, path, fmt, args);
  va_end(args);
}

static void validateWarn(ValidateStats& st, const std::string& path, const char* fmt, ...) {
  std::va_list args;
  va_start(args, fmt);
  validateVReport(st, false, path, fmt, args);
  va_end(args);
}

static bool validateStageToml(ValidateStats& st, const std::string& stagePath, StageMeta* outMeta) {
  Stage stage;
  ++st.stageFiles;
  const int startErrors = st.errors;
  if (!stage.loadFromToml(stagePath.c_str())) {
    validateError(st, stagePath, "failed to load stage TOML");
    return false;
  }

  if (outMeta) {
    outMeta->id = stage.id();
    outMeta->display = stage.displayName().empty() ? stage.id() : stage.displayName();
  }

  static constexpr int kExpectedVersion = 1;
  if (stage.version() != kExpectedVersion) {
    validateError(st, stagePath, "stage version %d (expected %d)", stage.version(),
                  kExpectedVersion);
    return false;
  }

  if (stage.id().empty()) {
    validateError(st, stagePath, "stage id is empty");
  }

  if (stage.solidCount() == 0 && stage.slopeCount() == 0) {
    validateError(st, stagePath,
                  "stage has no collision geometry (add at least one [[solids]] or [[slopes]])");
  }

  for (const auto& enemy : stage.enemySpawns()) {
    if (enemy.configPath.empty()) {
      validateError(st, stagePath, "enemy spawn missing config path");
      continue;
    }
    const std::string configPath = Paths::resolveAssetPath(enemy.configPath);
    if (!std::filesystem::exists(configPath)) {
      validateError(st, stagePath, "enemy config missing: %s", configPath.c_str());
      continue;
    }
    EnemyConfig cfg;
    if (!cfg.loadFromToml(configPath.c_str())) {
      validateError(st, stagePath, "enemy config failed to load: %s", configPath.c_str());
      continue;
    }
  }

  Rect world{};
  const bool hasWorld = stage.getWorldBounds(world);
  if (!hasWorld) {
    validateWarn(st, stagePath, "missing [world] width/height (world bounds will be unspecified)");
  }

  Rect camera{};
  const bool hasCamera = stage.getCameraBounds(camera);
  if (hasWorld && hasCamera) {
    const float wx2 = world.x + world.w;
    const float wy2 = world.y + world.h;
    const float cx2 = camera.x + camera.w;
    const float cy2 = camera.y + camera.h;
    if (camera.x < world.x || camera.y < world.y || cx2 > wx2 || cy2 > wy2) {
      validateWarn(st, stagePath, "camera bounds exceed world bounds");
    }
  }

  const std::vector<std::string> spawns = stage.spawnNames();
  for (const auto& name : spawns) {
    float x = 0.0F;
    float y = 0.0F;
    int facing = 1;
    if (!stage.getSpawn(name.c_str(), x, y, facing)) {
      validateWarn(st, stagePath, "spawn '%s' failed to resolve", name.c_str());
      continue;
    }

    if (hasWorld) {
      if (x < world.x || x > world.x + world.w || y < world.y || y > world.y + world.h) {
        validateWarn(st, stagePath, "spawn '%s' is out of world bounds (%.0f,%.0f)", name.c_str(),
                     x, y);
      }
    }
  }

  return st.errors == startErrors;
}

static void validateFallbackTestStageParity(ValidateStats& st, const char* argv0) {
  const std::string stagePath = Paths::resolveAssetPath("assets/dev/stages/test_stage.toml", argv0);
  Stage fromToml;
  if (!fromToml.loadFromToml(stagePath.c_str())) {
    validateWarn(st, stagePath, "failed to load (needed for fallback parity check)");
    return;
  }

  Stage fallback;
  fallback.loadTestStage();

  if (fallback.version() != fromToml.version()) {
    validateWarn(st, stagePath, "fallback version %d != TOML version %d", fallback.version(),
                 fromToml.version());
  }
  if (fallback.id() != fromToml.id()) {
    validateWarn(st, stagePath, "fallback id '%s' != TOML id '%s'", fallback.id().c_str(),
                 fromToml.id().c_str());
  }
  if (fallback.displayName() != fromToml.displayName()) {
    validateWarn(st, stagePath, "fallback display '%s' != TOML display '%s'",
                 fallback.displayName().c_str(), fromToml.displayName().c_str());
  }

  if (!nearlyEqual(fallback.collisionGroundSnap(), fromToml.collisionGroundSnap())) {
    validateWarn(st, stagePath, "fallback collision.ground_snap %.3f != TOML %.3f",
                 fallback.collisionGroundSnap(), fromToml.collisionGroundSnap());
  }
  if (!nearlyEqual(fallback.collisionStepUp(), fromToml.collisionStepUp())) {
    validateWarn(st, stagePath, "fallback collision.step_up %.3f != TOML %.3f",
                 fallback.collisionStepUp(), fromToml.collisionStepUp());
  }
  if (!nearlyEqual(fallback.collisionSkin(), fromToml.collisionSkin())) {
    validateWarn(st, stagePath, "fallback collision.skin %.3f != TOML %.3f",
                 fallback.collisionSkin(), fromToml.collisionSkin());
  }

  Rect fallbackWorld{};
  Rect tomlWorld{};
  const bool hasFallbackWorld = fallback.getWorldBounds(fallbackWorld);
  const bool hasTomlWorld = fromToml.getWorldBounds(tomlWorld);
  if (hasFallbackWorld != hasTomlWorld) {
    validateWarn(st, stagePath, "fallback world bounds presence differs from TOML");
  } else if (hasFallbackWorld) {
    if (!nearlyEqual(fallbackWorld.x, tomlWorld.x) || !nearlyEqual(fallbackWorld.y, tomlWorld.y) ||
        !nearlyEqual(fallbackWorld.w, tomlWorld.w) || !nearlyEqual(fallbackWorld.h, tomlWorld.h)) {
      validateWarn(st, stagePath,
                   "fallback world bounds (%.1f,%.1f,%.1f,%.1f) != TOML (%.1f,%.1f,%.1f,%.1f)",
                   fallbackWorld.x, fallbackWorld.y, fallbackWorld.w, fallbackWorld.h, tomlWorld.x,
                   tomlWorld.y, tomlWorld.w, tomlWorld.h);
    }
  }

  Rect fallbackCamera{};
  Rect tomlCamera{};
  const bool hasFallbackCamera = fallback.getCameraBounds(fallbackCamera);
  const bool hasTomlCamera = fromToml.getCameraBounds(tomlCamera);
  if (hasFallbackCamera != hasTomlCamera) {
    validateWarn(st, stagePath, "fallback camera bounds presence differs from TOML");
  } else if (hasFallbackCamera) {
    if (!nearlyEqual(fallbackCamera.x, tomlCamera.x) ||
        !nearlyEqual(fallbackCamera.y, tomlCamera.y) ||
        !nearlyEqual(fallbackCamera.w, tomlCamera.w) ||
        !nearlyEqual(fallbackCamera.h, tomlCamera.h)) {
      validateWarn(st, stagePath,
                   "fallback camera bounds (%.1f,%.1f,%.1f,%.1f) != TOML (%.1f,%.1f,%.1f,%.1f)",
                   fallbackCamera.x, fallbackCamera.y, fallbackCamera.w, fallbackCamera.h,
                   tomlCamera.x, tomlCamera.y, tomlCamera.w, tomlCamera.h);
    }
  }

  if (!nearlyEqual(fallback.cameraDeadzoneW(), fromToml.cameraDeadzoneW()) ||
      !nearlyEqual(fallback.cameraDeadzoneH(), fromToml.cameraDeadzoneH()) ||
      !nearlyEqual(fallback.cameraLookaheadX(), fromToml.cameraLookaheadX()) ||
      !nearlyEqual(fallback.cameraLookaheadY(), fromToml.cameraLookaheadY())) {
    validateWarn(st, stagePath,
                 "fallback camera tuning (deadzone=%.1fx%.1f lookahead=%.1f,%.1f) != TOML "
                 "(deadzone=%.1fx%.1f lookahead=%.1f,%.1f)",
                 fallback.cameraDeadzoneW(), fallback.cameraDeadzoneH(),
                 fallback.cameraLookaheadX(), fallback.cameraLookaheadY(),
                 fromToml.cameraDeadzoneW(), fromToml.cameraDeadzoneH(),
                 fromToml.cameraLookaheadX(), fromToml.cameraLookaheadY());
  }

  const StageRenderStyle fallbackRender = fallback.renderStyle();
  const StageRenderStyle tomlRender = fromToml.renderStyle();
  if (fallbackRender.bgTop.r != tomlRender.bgTop.r ||
      fallbackRender.bgTop.g != tomlRender.bgTop.g ||
      fallbackRender.bgTop.b != tomlRender.bgTop.b ||
      fallbackRender.bgBottom.r != tomlRender.bgBottom.r ||
      fallbackRender.bgBottom.g != tomlRender.bgBottom.g ||
      fallbackRender.bgBottom.b != tomlRender.bgBottom.b ||
      fallbackRender.platformBase.r != tomlRender.platformBase.r ||
      fallbackRender.platformBase.g != tomlRender.platformBase.g ||
      fallbackRender.platformBase.b != tomlRender.platformBase.b ||
      fallbackRender.platformLight.r != tomlRender.platformLight.r ||
      fallbackRender.platformLight.g != tomlRender.platformLight.g ||
      fallbackRender.platformLight.b != tomlRender.platformLight.b ||
      fallbackRender.platformDark.r != tomlRender.platformDark.r ||
      fallbackRender.platformDark.g != tomlRender.platformDark.g ||
      fallbackRender.platformDark.b != tomlRender.platformDark.b ||
      fallbackRender.platformHighlight.r != tomlRender.platformHighlight.r ||
      fallbackRender.platformHighlight.g != tomlRender.platformHighlight.g ||
      fallbackRender.platformHighlight.b != tomlRender.platformHighlight.b) {
    validateWarn(st, stagePath, "fallback render palette differs from TOML");
  }

  if (fallback.solidCount() != fromToml.solidCount()) {
    validateWarn(st, stagePath, "fallback solid count %zu != TOML %zu", fallback.solidCount(),
                 fromToml.solidCount());
  }
  if (fallback.slopeCount() != fromToml.slopeCount()) {
    validateWarn(st, stagePath, "fallback slope count %zu != TOML %zu", fallback.slopeCount(),
                 fromToml.slopeCount());
  }

  std::vector<std::string> fallbackSpawns = fallback.spawnNames();
  std::vector<std::string> tomlSpawns = fromToml.spawnNames();
  std::sort(fallbackSpawns.begin(), fallbackSpawns.end());
  std::sort(tomlSpawns.begin(), tomlSpawns.end());
  if (fallbackSpawns != tomlSpawns) {
    validateWarn(st, stagePath, "fallback spawn set differs from TOML");
  }

  for (const auto& name : tomlSpawns) {
    float fx = 0.0F;
    float fy = 0.0F;
    int ffacing = 1;
    float tx = 0.0F;
    float ty = 0.0F;
    int tfacing = 1;
    const bool hasFallback = fallback.getSpawn(name.c_str(), fx, fy, ffacing);
    const bool hasToml = fromToml.getSpawn(name.c_str(), tx, ty, tfacing);
    if (hasFallback != hasToml) {
      validateWarn(st, stagePath, "spawn '%s' presence differs between fallback and TOML",
                   name.c_str());
      continue;
    }
    if (!hasToml)
      continue;

    const int fFacingNorm = normalizeFacing(ffacing);
    const int tFacingNorm = normalizeFacing(tfacing);
    if (!nearlyEqual(fx, tx) || !nearlyEqual(fy, ty) || fFacingNorm != tFacingNorm) {
      validateWarn(st, stagePath, "spawn '%s' fallback (%.1f,%.1f,%d) != TOML (%.1f,%.1f,%d)",
                   name.c_str(), fx, fy, fFacingNorm, tx, ty, tFacingNorm);
    }
  }
}

static bool validateInputScriptToml(ValidateStats& st, const std::string& scriptPath) {
  ++st.inputScriptFiles;
  const int startErrors = st.errors;

  InputScript script;
  if (!script.loadFromToml(scriptPath.c_str())) {
    validateError(st, scriptPath, "failed to load input script TOML");
    return false;
  }

  return st.errors == startErrors;
}

static bool validateCharacterToml(ValidateStats& st,
                                  const std::string& characterPath,
                                  const char* argv0,
                                  CharacterMeta* outMeta) {
  CharacterConfig cfg;
  ++st.characterFiles;
  const int startErrors = st.errors;
  if (!cfg.loadFromToml(characterPath.c_str())) {
    validateError(st, characterPath, "failed to load character TOML");
    return false;
  }

  if (outMeta) {
    outMeta->id = cfg.id;
    outMeta->display = cfg.displayName.empty() ? cfg.id : cfg.displayName;
  }

  static constexpr int kExpectedVersion = 1;
  if (cfg.version != kExpectedVersion) {
    validateError(st, characterPath, "character version %d (expected %d)", cfg.version,
                  kExpectedVersion);
    return false;
  }

  if (cfg.id.empty()) {
    validateError(st, characterPath, "character id is empty");
  }

  if (!cfg.form.id.empty()) {
    const std::string formPath =
        Paths::resolveAssetPath("assets/forms/" + cfg.form.id + ".toml", argv0);
    Visual::CharacterForm form;
    if (Visual::loadFormFromToml(formPath.c_str(), form)) {
      auto variantsMatch = [](const std::vector<std::string>& shapeKeys,
                              const std::vector<std::string>& variants) {
        for (const std::string& key : shapeKeys) {
          if (std::find(variants.begin(), variants.end(), key) != variants.end()) {
            return true;
          }
        }
        return false;
      };
      auto shapeEnabled = [&](const Visual::FormShape& shape) {
        if (!shape.onlyInVariants.empty()) {
          if (!variantsMatch(shape.onlyInVariants, cfg.form.variants)) {
            return false;
          }
        }
        if (!shape.hiddenInVariants.empty()) {
          if (variantsMatch(shape.hiddenInVariants, cfg.form.variants)) {
            return false;
          }
        }
        return true;
      };

      std::unordered_set<std::string> missing;
      for (const Visual::FormShape& shape : form.shapes) {
        if (!shapeEnabled(shape)) {
          continue;
        }
        if (!shape.colorKey.empty()) {
          if (cfg.form.colors.find(shape.colorKey) == cfg.form.colors.end()) {
            missing.insert(shape.colorKey);
          }
        }
        for (const auto& [_, key] : shape.pixelColorKeys) {
          if (cfg.form.colors.find(key) == cfg.form.colors.end()) {
            missing.insert(key);
          }
        }
      }
      if (!missing.empty()) {
        std::vector<std::string> keys;
        keys.reserve(missing.size());
        for (const auto& key : missing) {
          keys.push_back(key);
        }
        std::sort(keys.begin(), keys.end());
        std::string joined;
        for (const auto& key : keys) {
          if (!joined.empty()) {
            joined += ", ";
          }
          joined += key;
        }
        validateWarn(st, characterPath, "form color keys missing: %s", joined.c_str());
      }
    } else {
      validateWarn(st, characterPath, "failed to load form TOML '%s' for color key validation",
                   formPath.c_str());
    }
  }

  if (!cfg.render.sheet.empty()) {
    if (cfg.render.frameW <= 0 || cfg.render.frameH <= 0) {
      validateError(st, characterPath,
                    "render.sheet set but frame_w/frame_h are missing or invalid");
    }

    const std::string sheetPath = Paths::resolveAssetPath(cfg.render.sheet, argv0);
    if (!std::filesystem::exists(sheetPath)) {
      validateWarn(st, characterPath, "missing sprite sheet: %s", sheetPath.c_str());
    }
  }

  if (cfg.render.scale <= 0.0F) {
    validateWarn(st, characterPath, "render.scale <= 0 (will default to 1.0 at runtime)");
  }

  if (cfg.physics.gravity <= 0.0F)
    validateError(st, characterPath, "physics.gravity must be > 0");
  if (cfg.physics.maxFallSpeed <= 0.0F)
    validateError(st, characterPath, "physics.max_fall_speed must be > 0");
  if (cfg.move.maxSpeedGround <= 0.0F)
    validateError(st, characterPath, "move.max_speed_ground must be > 0");
  if (cfg.move.maxSpeedAir <= 0.0F)
    validateError(st, characterPath, "move.max_speed_air must be > 0");
  if (cfg.math.subpixel < 0)
    validateError(st, characterPath, "math.subpixel must be >= 0");
  if (cfg.math.subpixel == 1)
    validateWarn(st, characterPath, "math.subpixel=1 has no effect (use 0 or >= 2)");
  if (cfg.jump.enabled && cfg.jump.impulse <= 0.0F)
    validateError(st, characterPath, "jump.impulse must be > 0");
  if (cfg.jump.coyoteFrames < 0)
    validateError(st, characterPath, "jump.coyote_time_ms must be >= 0");
  if (cfg.jump.jumpBufferFrames < 0)
    validateError(st, characterPath, "jump.jump_buffer_ms must be >= 0");
  if (cfg.jump.maxHoldFrames < 0)
    validateError(st, characterPath, "jump.max_hold_frames must be >= 0");
  if (cfg.jump.releaseDropAfterFrames < 0)
    validateError(st, characterPath, "jump.release_drop_after_ms must be >= 0");
  if (cfg.jump.releaseDrop < 0.0F)
    validateError(st, characterPath, "jump.release_drop must be >= 0");
  if (cfg.jump.variableCutMultiplier < 0.0F || cfg.jump.variableCutMultiplier > 1.0F) {
    validateWarn(st, characterPath, "jump.variable_cut_multiplier is outside [0..1]");
  }

  if (cfg.jump.model == CharacterConfig::JumpModel::DualGravity) {
    if (cfg.jump.riseGravityMultiplierHeld < 0.0F) {
      validateError(st, characterPath, "jump.rise_gravity_multiplier_held must be >= 0");
    }
    if (cfg.jump.riseGravityMultiplierReleased < 0.0F) {
      validateError(st, characterPath, "jump.rise_gravity_multiplier_released must be >= 0");
    }
  }

  if (cfg.jump.model == CharacterConfig::JumpModel::ClampVy && cfg.jump.variableJump) {
    if (cfg.jump.releaseClampVy <= 0.0F) {
      validateWarn(
          st, characterPath,
          "jump.model=clamp_vy but jump.release_clamp_vy <= 0 (early release will have no effect)");
    }
  }

  if (cfg.collision.w <= 0.0F || cfg.collision.h <= 0.0F) {
    validateError(st, characterPath, "collision w/h must be > 0");
  }

  if (cfg.actions.dash.enabled) {
    if (cfg.actions.dash.dashTimeFrames <= 0)
      validateError(st, characterPath, "actions.dash.dash_time_ms must be > 0");
    if (cfg.actions.dash.dashSpeed <= 0.0F)
      validateError(st, characterPath, "actions.dash.dash_speed must be > 0");
    if (cfg.actions.dash.cooldownFrames < 0)
      validateError(st, characterPath, "actions.dash.cooldown_ms must be >= 0");
    if (cfg.actions.dash.airDashes < -1)
      validateError(st, characterPath, "actions.dash.air_dashes must be -1 or >= 0");
  }

  if (cfg.actions.spindash.enabled) {
    if (cfg.actions.spindash.chargeFrames <= 0)
      validateError(st, characterPath, "actions.spindash.charge_time_ms must be > 0");
    if (cfg.actions.spindash.minLaunchSpeed < 0.0F)
      validateError(st, characterPath, "actions.spindash.min_launch_speed must be >= 0");
    if (cfg.actions.spindash.maxLaunchSpeed < cfg.actions.spindash.minLaunchSpeed) {
      validateError(st, characterPath,
                    "actions.spindash.max_launch_speed must be >= min_launch_speed");
    }
    if (cfg.actions.spindash.tapBoostFrames < 0) {
      validateWarn(st, characterPath,
                   "actions.spindash.tap_boost_ms must be >= 0 (got {}); clamping to 0",
                   cfg.actions.spindash.tapBoostFrames);
    }
  }

  if (cfg.actions.spin.enabled) {
    if (cfg.actions.spin.minSpeed < 0.0F)
      validateError(st, characterPath, "actions.spin.min_speed must be >= 0");
    if (cfg.actions.spin.spinFriction <= 0.0F)
      validateWarn(st, characterPath, "actions.spin.spin_friction <= 0 (spin will have no effect)");
  }

  if (cfg.actions.fly.enabled) {
    if (cfg.actions.fly.upAccel <= 0.0F)
      validateError(st, characterPath, "actions.fly.up_accel must be > 0");
    if (cfg.actions.fly.maxUpSpeed <= 0.0F)
      validateError(st, characterPath, "actions.fly.max_up_speed must be > 0");
  }

  if (cfg.actions.glide.enabled) {
    if (cfg.actions.glide.gravityMultiplier < 0.0F || cfg.actions.glide.gravityMultiplier > 1.0F) {
      validateWarn(st, characterPath, "actions.glide.gravity_multiplier is outside [0..1]");
    }
    if (cfg.actions.glide.maxFallSpeed <= 0.0F)
      validateError(st, characterPath, "actions.glide.max_fall_speed must be > 0");
  }

  return st.errors == startErrors;
}

static bool validateEnemyToml(ValidateStats& st, const std::string& enemyPath, EnemyMeta* outMeta) {
  EnemyConfig cfg;
  ++st.enemyFiles;
  const int startErrors = st.errors;
  if (!cfg.loadFromToml(enemyPath.c_str())) {
    validateError(st, enemyPath, "failed to load enemy TOML");
    return false;
  }

  if (outMeta) {
    outMeta->id = cfg.id;
    outMeta->display = cfg.displayName.empty() ? cfg.id : cfg.displayName;
  }

  static constexpr int kExpectedVersion = 1;
  if (cfg.version != kExpectedVersion) {
    validateError(st, enemyPath, "enemy version %d (expected %d)", cfg.version, kExpectedVersion);
  }

  if (cfg.id.empty()) {
    validateError(st, enemyPath, "enemy id is empty");
  }

  if (cfg.collision.w <= 0.0F || cfg.collision.h <= 0.0F) {
    validateError(st, enemyPath, "collision w/h must be > 0");
  }

  if (cfg.move.speed < 0.0F) {
    validateError(st, enemyPath, "move.speed must be >= 0");
  }
  if (cfg.move.gravity < 0.0F) {
    validateError(st, enemyPath, "move.gravity must be >= 0");
  }
  if (cfg.move.maxFallSpeed < 0.0F) {
    validateError(st, enemyPath, "move.max_fall_speed must be >= 0");
  }

  if (cfg.combat.health <= 0) {
    validateError(st, enemyPath, "combat.health must be > 0");
  }
  if (cfg.combat.contactDamage < 0.0F) {
    validateError(st, enemyPath, "combat.contact_damage must be >= 0");
  }
  if (cfg.combat.iframesMs < 0) {
    validateError(st, enemyPath, "combat.iframes_ms must be >= 0");
  }

  if (cfg.type == EnemyConfig::Type::Hopper) {
    if (cfg.hopper.intervalFrames <= 0) {
      validateError(st, enemyPath, "hopper.interval_frames must be > 0");
    }
  }

  if (cfg.type == EnemyConfig::Type::Shooter) {
    if (cfg.shooter.fireIntervalFrames <= 0) {
      validateError(st, enemyPath, "shooter.fire_interval_frames must be > 0");
    }
    if (cfg.shooter.projectile.lifetimeFrames <= 0) {
      validateError(st, enemyPath, "shooter.projectile.lifetime_frames must be > 0");
    }
    if (cfg.shooter.projectile.w <= 0.0F || cfg.shooter.projectile.h <= 0.0F) {
      validateError(st, enemyPath, "shooter.projectile w/h must be > 0");
    }
  }

  return st.errors == startErrors;
}

static bool validateFormToml(ValidateStats& st, const std::string& formPath, FormMeta* outMeta) {
  ++st.formFiles;
  const int startErrors = st.errors;
  Visual::CharacterForm form;
  if (!Visual::loadFormFromToml(formPath.c_str(), form)) {
    validateError(st, formPath, "failed to load form TOML");
    return false;
  }

  if (outMeta)
    outMeta->id = form.id;

  if (form.id.empty()) {
    validateError(st, formPath, "form id is empty");
  }

  return st.errors == startErrors;
}

static std::string findStageTomlById(std::string_view stageId, const char* argv0) {
  const std::string dir = Paths::resolveAssetPath("assets/stages", argv0);
  const std::vector<std::string> paths = Paths::listTomlFiles(dir);

  std::string found;
  Stage stage;
  for (const auto& p : paths) {
    if (!stage.loadFromToml(p.c_str()))
      continue;
    if (stage.id() != stageId)
      continue;

    if (found.empty())
      found = p;
    else
      return std::string{};
  }

  return found;
}

static std::string findCharacterTomlById(std::string_view characterId, const char* argv0) {
  const std::string dir = Paths::resolveAssetPath("assets/characters", argv0);
  const std::vector<std::string> paths = Paths::listTomlFiles(dir);

  std::string found;
  CharacterConfig cfg;
  for (const auto& p : paths) {
    if (!cfg.loadFromToml(p.c_str()))
      continue;
    if (cfg.id != characterId)
      continue;

    if (found.empty())
      found = p;
    else
      return std::string{};
  }

  return found;
}

int main(int argc, char** argv) {
  int maxFrames = -1;
  const char* videoDriver = nullptr;
  AppConfig cfg{};
  bool expectGrounded = false;
  bool expectNoRespawn = false;
  bool expectMinX = false;
  float expectedMinX = 0.0F;
  bool expectMaxX = false;
  float expectedMaxX = 0.0F;
  bool expectMinVx = false;
  float expectedMinVx = 0.0F;
  bool expectMaxVx = false;
  float expectedMaxVx = 0.0F;
  bool expectMinY = false;
  float expectedMinY = 0.0F;
  bool expectMaxY = false;
  float expectedMaxY = 0.0F;
  bool expectMinCamX = false;
  float expectedMinCamX = 0.0F;
  bool expectMaxCamX = false;
  float expectedMaxCamX = 0.0F;
  bool expectMinCamY = false;
  float expectedMinCamY = 0.0F;
  bool expectMaxCamY = false;
  float expectedMaxCamY = 0.0F;
  bool expectNoCameraBackscroll = false;
  bool expectHurtCount = false;
  int expectedHurtCount = 0;
  bool expectEnemyKills = false;
  int expectedEnemyKills = 0;
  bool listStages = false;
  bool listStageIds = false;
  bool listCharacters = false;
  bool listCharacterIds = false;
  bool listSpawns = false;
  bool validate = false;
  bool strict = false;
  const char* stageId = nullptr;
  const char* characterId = nullptr;
  std::string stagePathById;
  std::string characterPathById;

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
      usage(argv[0]);
      return 0;
    }
    if (std::strcmp(argv[i], "--frames") == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        return 2;
      }
      maxFrames = std::atoi(argv[++i]);
      continue;
    }
    if (std::strcmp(argv[i], "--video-driver") == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        return 2;
      }
      videoDriver = argv[++i];
      continue;
    }
    if (std::strcmp(argv[i], "--stage") == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        return 2;
      }
      cfg.stageTomlPath = argv[++i];
      continue;
    }
    if (std::strcmp(argv[i], "--stage-id") == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        return 2;
      }
      stageId = argv[++i];
      continue;
    }
    if (std::strcmp(argv[i], "--character") == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        return 2;
      }
      cfg.characterTomlPath = argv[++i];
      continue;
    }
    if (std::strcmp(argv[i], "--character-id") == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        return 2;
      }
      characterId = argv[++i];
      continue;
    }
    if (std::strcmp(argv[i], "--spawn-point") == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        return 2;
      }
      cfg.spawnPoint = argv[++i];
      cfg.spawnOverride = false;
      continue;
    }
    if (std::strcmp(argv[i], "--spawn") == 0) {
      if (i + 2 >= argc) {
        usage(argv[0]);
        return 2;
      }

      float x = 0.0F;
      float y = 0.0F;
      ++i;
      const char* xArg = argv[i];
      ++i;
      const char* yArg = argv[i];
      if (!parseFloat(xArg, x) || !parseFloat(yArg, y)) {
        std::printf("invalid --spawn values\n");
        return 2;
      }
      cfg.spawnX = x;
      cfg.spawnY = y;
      cfg.spawnPoint = nullptr;
      cfg.spawnOverride = true;
      continue;
    }
    if (std::strcmp(argv[i], "--input-script") == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        return 2;
      }
      cfg.inputScriptTomlPath = argv[++i];
      continue;
    }
    if (std::strcmp(argv[i], "--no-ui") == 0) {
      cfg.noUi = true;
      continue;
    }
    if (std::strcmp(argv[i], "--no-prefs") == 0) {
      cfg.noPrefs = true;
      continue;
    }
    if (std::strcmp(argv[i], "--no-render") == 0) {
      cfg.noRender = true;
      continue;
    }
    if (std::strcmp(argv[i], "--expect-grounded") == 0) {
      expectGrounded = true;
      continue;
    }
    if (std::strcmp(argv[i], "--expect-no-respawn") == 0) {
      expectNoRespawn = true;
      continue;
    }
    if (std::strcmp(argv[i], "--expect-min-x") == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        return 2;
      }
      ++i;
      const char* xArg = argv[i];
      float x = 0.0F;
      if (!parseFloat(xArg, x)) {
        std::printf("invalid --expect-min-x value\n");
        return 2;
      }
      expectMinX = true;
      expectedMinX = x;
      continue;
    }
    if (std::strcmp(argv[i], "--expect-max-x") == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        return 2;
      }
      ++i;
      const char* xArg = argv[i];
      float x = 0.0F;
      if (!parseFloat(xArg, x)) {
        std::printf("invalid --expect-max-x value\n");
        return 2;
      }
      expectMaxX = true;
      expectedMaxX = x;
      continue;
    }
    if (std::strcmp(argv[i], "--expect-min-vx") == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        return 2;
      }
      ++i;
      const char* xArg = argv[i];
      float x = 0.0F;
      if (!parseFloat(xArg, x)) {
        std::printf("invalid --expect-min-vx value\n");
        return 2;
      }
      expectMinVx = true;
      expectedMinVx = x;
      continue;
    }
    if (std::strcmp(argv[i], "--expect-max-vx") == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        return 2;
      }
      ++i;
      const char* xArg = argv[i];
      float x = 0.0F;
      if (!parseFloat(xArg, x)) {
        std::printf("invalid --expect-max-vx value\n");
        return 2;
      }
      expectMaxVx = true;
      expectedMaxVx = x;
      continue;
    }
    if (std::strcmp(argv[i], "--expect-min-y") == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        return 2;
      }
      ++i;
      const char* yArg = argv[i];
      float y = 0.0F;
      if (!parseFloat(yArg, y)) {
        std::printf("invalid --expect-min-y value\n");
        return 2;
      }
      expectMinY = true;
      expectedMinY = y;
      continue;
    }
    if (std::strcmp(argv[i], "--expect-max-y") == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        return 2;
      }
      ++i;
      const char* yArg = argv[i];
      float y = 0.0F;
      if (!parseFloat(yArg, y)) {
        std::printf("invalid --expect-max-y value\n");
        return 2;
      }
      expectMaxY = true;
      expectedMaxY = y;
      continue;
    }
    if (std::strcmp(argv[i], "--expect-min-cam-x") == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        return 2;
      }
      ++i;
      const char* xArg = argv[i];
      float x = 0.0F;
      if (!parseFloat(xArg, x)) {
        std::printf("invalid --expect-min-cam-x value\n");
        return 2;
      }
      expectMinCamX = true;
      expectedMinCamX = x;
      continue;
    }
    if (std::strcmp(argv[i], "--expect-max-cam-x") == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        return 2;
      }
      ++i;
      const char* xArg = argv[i];
      float x = 0.0F;
      if (!parseFloat(xArg, x)) {
        std::printf("invalid --expect-max-cam-x value\n");
        return 2;
      }
      expectMaxCamX = true;
      expectedMaxCamX = x;
      continue;
    }
    if (std::strcmp(argv[i], "--expect-min-cam-y") == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        return 2;
      }
      ++i;
      const char* yArg = argv[i];
      float y = 0.0F;
      if (!parseFloat(yArg, y)) {
        std::printf("invalid --expect-min-cam-y value\n");
        return 2;
      }
      expectMinCamY = true;
      expectedMinCamY = y;
      continue;
    }
    if (std::strcmp(argv[i], "--expect-max-cam-y") == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        return 2;
      }
      ++i;
      const char* yArg = argv[i];
      float y = 0.0F;
      if (!parseFloat(yArg, y)) {
        std::printf("invalid --expect-max-cam-y value\n");
        return 2;
      }
      expectMaxCamY = true;
      expectedMaxCamY = y;
      continue;
    }
    if (std::strcmp(argv[i], "--expect-no-camera-backscroll") == 0) {
      expectNoCameraBackscroll = true;
      continue;
    }
    if (std::strcmp(argv[i], "--expect-hurt-count") == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        return 2;
      }
      ++i;
      const char* nArg = argv[i];
      int n = 0;
      if (!parseInt(nArg, n) || n < 0) {
        std::printf("invalid --expect-hurt-count value\n");
        return 2;
      }
      expectHurtCount = true;
      expectedHurtCount = n;
      continue;
    }
    if (std::strcmp(argv[i], "--expect-enemy-kills") == 0) {
      if (i + 1 >= argc) {
        usage(argv[0]);
        return 2;
      }
      ++i;
      const char* nArg = argv[i];
      int n = 0;
      if (!parseInt(nArg, n) || n < 0) {
        std::printf("invalid --expect-enemy-kills value\n");
        return 2;
      }
      expectEnemyKills = true;
      expectedEnemyKills = n;
      continue;
    }
    if (std::strcmp(argv[i], "--list-stages") == 0) {
      listStages = true;
      continue;
    }
    if (std::strcmp(argv[i], "--list-stage-ids") == 0) {
      listStageIds = true;
      continue;
    }
    if (std::strcmp(argv[i], "--list-characters") == 0) {
      listCharacters = true;
      continue;
    }
    if (std::strcmp(argv[i], "--list-character-ids") == 0) {
      listCharacterIds = true;
      continue;
    }
    if (std::strcmp(argv[i], "--list-spawns") == 0) {
      listSpawns = true;
      continue;
    }
    if (std::strcmp(argv[i], "--validate") == 0) {
      validate = true;
      continue;
    }
    if (std::strcmp(argv[i], "--strict") == 0) {
      strict = true;
      continue;
    }

    std::printf("unknown arg: %s\n", argv[i]);
    usage(argv[0]);
    return 2;
  }

  const int modeCount = (listStages ? 1 : 0) + (listStageIds ? 1 : 0) + (listCharacters ? 1 : 0) +
                        (listCharacterIds ? 1 : 0) + (listSpawns ? 1 : 0) + (validate ? 1 : 0);
  if (modeCount > 1) {
    std::printf(
        "choose only one of: --list-stages, --list-stage-ids, --list-characters, "
        "--list-character-ids, --list-spawns, --validate\n");
    return 2;
  }

  if (strict && !validate) {
    std::printf("--strict requires --validate\n");
    return 2;
  }

  if (expectGrounded && maxFrames < 0) {
    std::printf("--expect-grounded requires --frames\n");
    return 2;
  }
  if (expectNoRespawn && maxFrames < 0) {
    std::printf("--expect-no-respawn requires --frames\n");
    return 2;
  }
  if (expectMinX && maxFrames < 0) {
    std::printf("--expect-min-x requires --frames\n");
    return 2;
  }
  if (expectMaxX && maxFrames < 0) {
    std::printf("--expect-max-x requires --frames\n");
    return 2;
  }
  if (expectMinVx && maxFrames < 0) {
    std::printf("--expect-min-vx requires --frames\n");
    return 2;
  }
  if (expectMaxVx && maxFrames < 0) {
    std::printf("--expect-max-vx requires --frames\n");
    return 2;
  }
  if (expectMinY && maxFrames < 0) {
    std::printf("--expect-min-y requires --frames\n");
    return 2;
  }
  if (expectMaxY && maxFrames < 0) {
    std::printf("--expect-max-y requires --frames\n");
    return 2;
  }
  if (expectMinCamX && maxFrames < 0) {
    std::printf("--expect-min-cam-x requires --frames\n");
    return 2;
  }
  if (expectMaxCamX && maxFrames < 0) {
    std::printf("--expect-max-cam-x requires --frames\n");
    return 2;
  }
  if (expectMinCamY && maxFrames < 0) {
    std::printf("--expect-min-cam-y requires --frames\n");
    return 2;
  }
  if (expectMaxCamY && maxFrames < 0) {
    std::printf("--expect-max-cam-y requires --frames\n");
    return 2;
  }
  if (expectNoCameraBackscroll && maxFrames < 0) {
    std::printf("--expect-no-camera-backscroll requires --frames\n");
    return 2;
  }
  if (expectHurtCount && maxFrames < 0) {
    std::printf("--expect-hurt-count requires --frames\n");
    return 2;
  }
  if (expectEnemyKills && maxFrames < 0) {
    std::printf("--expect-enemy-kills requires --frames\n");
    return 2;
  }

  if (stageId && cfg.stageTomlPath) {
    std::printf("choose only one of: --stage or --stage-id\n");
    return 2;
  }
  if (characterId && cfg.characterTomlPath) {
    std::printf("choose only one of: --character or --character-id\n");
    return 2;
  }

  if (stageId) {
    stagePathById = findStageTomlById(stageId, argv[0]);
    if (stagePathById.empty()) {
      std::fprintf(stderr, "stage id not found (or ambiguous): %s\n", stageId);
      return 1;
    }
    cfg.stageTomlPath = stagePathById.c_str();
  }
  if (characterId) {
    characterPathById = findCharacterTomlById(characterId, argv[0]);
    if (characterPathById.empty()) {
      std::fprintf(stderr, "character id not found (or ambiguous): %s\n", characterId);
      return 1;
    }
    cfg.characterTomlPath = characterPathById.c_str();
  }

  if (listStages) {
    const std::string dir = Paths::resolveAssetPath("assets/stages", argv[0]);
    for (const auto& p : Paths::listTomlFiles(dir))
      std::printf("%s\n", p.c_str());
    return 0;
  }

  if (listStageIds) {
    const std::string dir = Paths::resolveAssetPath("assets/stages", argv[0]);
    const std::vector<std::string> paths = Paths::listTomlFiles(dir);
    bool ok = true;
    for (const auto& p : paths) {
      Stage stage;
      if (!stage.loadFromToml(p.c_str())) {
        std::fprintf(stderr, "failed to load stage TOML: %s\n", p.c_str());
        ok = false;
        continue;
      }

      std::string display = stage.displayName();
      if (display.empty())
        display = stage.id();
      std::printf("%s\t%s\t%s\n", stage.id().c_str(), display.c_str(), p.c_str());
    }
    return ok ? 0 : 1;
  }

  if (listCharacters) {
    const std::string dir = Paths::resolveAssetPath("assets/characters", argv[0]);
    for (const auto& p : Paths::listTomlFiles(dir))
      std::printf("%s\n", p.c_str());
    return 0;
  }

  if (listCharacterIds) {
    const std::string dir = Paths::resolveAssetPath("assets/characters", argv[0]);
    const std::vector<std::string> paths = Paths::listTomlFiles(dir);
    bool ok = true;
    for (const auto& p : paths) {
      CharacterConfig cfg2;
      if (!cfg2.loadFromToml(p.c_str())) {
        std::fprintf(stderr, "failed to load character TOML: %s\n", p.c_str());
        ok = false;
        continue;
      }

      const std::string display = cfg2.displayName.empty() ? cfg2.id : cfg2.displayName;
      std::printf("%s\t%s\t%s\n", cfg2.id.c_str(), display.c_str(), p.c_str());
    }
    return ok ? 0 : 1;
  }

  if (listSpawns) {
    const char* stageArg = cfg.stageTomlPath ? cfg.stageTomlPath : kDefaultStageToml;
    const std::string stagePath = Paths::resolveAssetPath(stageArg, argv[0]);
    Stage stage;
    if (!stage.loadFromToml(stagePath.c_str())) {
      std::printf("Failed to load stage TOML: %s\n", stagePath.c_str());
      return 1;
    }

    for (const auto& name : stage.spawnNames()) {
      float x = 0.0F;
      float y = 0.0F;
      int facing = 1;
      if (!stage.getSpawn(name.c_str(), x, y, facing))
        continue;
      std::printf("%s\t%.1f\t%.1f\t%d\n", name.c_str(), x, y, (facing < 0) ? -1 : 1);
    }
    return 0;
  }

  if (validate) {
    TomlUtil::resetWarningCount();
    ValidateStats st{};
    bool ok = true;
    const bool hasStageArg = (cfg.stageTomlPath != nullptr);
    const bool hasCharacterArg = (cfg.characterTomlPath != nullptr);
    const bool hasInputScriptArg = (cfg.inputScriptTomlPath != nullptr);

    if (hasStageArg) {
      const std::string stagePath = Paths::resolveAssetPath(cfg.stageTomlPath, argv[0]);
      ok = validateStageToml(st, stagePath, nullptr) && ok;
    }
    if (hasCharacterArg) {
      const std::string charPath = Paths::resolveAssetPath(cfg.characterTomlPath, argv[0]);
      ok = validateCharacterToml(st, charPath, argv[0], nullptr) && ok;
    }
    if (hasInputScriptArg) {
      const std::string scriptPath = Paths::resolveAssetPath(cfg.inputScriptTomlPath, argv[0]);
      ok = validateInputScriptToml(st, scriptPath) && ok;
    }

    if (!hasStageArg && !hasCharacterArg && !hasInputScriptArg) {
      std::unordered_map<std::string, std::string> stageIdToPath;
      std::unordered_map<std::string, std::string> stageDisplayToPath;
      std::unordered_map<std::string, std::string> characterIdToPath;
      std::unordered_map<std::string, std::string> characterDisplayToPath;
      std::unordered_map<std::string, std::string> enemyIdToPath;
      std::unordered_map<std::string, std::string> enemyDisplayToPath;
      std::unordered_map<std::string, std::string> formIdToPath;

      const std::string stageDir = Paths::resolveAssetPath("assets/stages", argv[0]);
      const std::vector<std::string> stages = Paths::listTomlFiles(stageDir);
      if (stages.empty()) {
        validateError(st, stageDir, "no stage TOMLs found");
        ok = false;
      }
      for (const auto& p : stages) {
        StageMeta meta{};
        ok = validateStageToml(st, p, &meta) && ok;
        if (!meta.id.empty()) {
          auto [it, inserted] = stageIdToPath.emplace(meta.id, p);
          if (!inserted && it->second != p) {
            validateError(st, p, "duplicate stage id '%s' (also in %s)", meta.id.c_str(),
                          it->second.c_str());
            ok = false;
          }
        }
        if (!meta.display.empty()) {
          auto [it, inserted] = stageDisplayToPath.emplace(meta.display, p);
          if (!inserted && it->second != p) {
            validateWarn(st, p, "duplicate stage display '%s' (also in %s)", meta.display.c_str(),
                         it->second.c_str());
          }
        }
      }

      const std::string charDir = Paths::resolveAssetPath("assets/characters", argv[0]);
      const std::vector<std::string> chars = Paths::listTomlFiles(charDir);
      if (chars.empty()) {
        validateError(st, charDir, "no character TOMLs found");
        ok = false;
      }
      for (const auto& p : chars) {
        CharacterMeta meta{};
        ok = validateCharacterToml(st, p, argv[0], &meta) && ok;
        if (!meta.id.empty()) {
          auto [it, inserted] = characterIdToPath.emplace(meta.id, p);
          if (!inserted && it->second != p) {
            validateError(st, p, "duplicate character id '%s' (also in %s)", meta.id.c_str(),
                          it->second.c_str());
            ok = false;
          }
        }
        if (!meta.display.empty()) {
          auto [it, inserted] = characterDisplayToPath.emplace(meta.display, p);
          if (!inserted && it->second != p) {
            validateWarn(st, p, "duplicate character display '%s' (also in %s)",
                         meta.display.c_str(), it->second.c_str());
          }
        }
      }

      const std::string enemyDir = Paths::resolveAssetPath("assets/enemies", argv[0]);
      const std::vector<std::string> enemies = Paths::listTomlFiles(enemyDir);
      if (enemies.empty()) {
        validateError(st, enemyDir, "no enemy TOMLs found");
        ok = false;
      }
      for (const auto& p : enemies) {
        EnemyMeta meta{};
        ok = validateEnemyToml(st, p, &meta) && ok;
        if (!meta.id.empty()) {
          auto [it, inserted] = enemyIdToPath.emplace(meta.id, p);
          if (!inserted && it->second != p) {
            validateError(st, p, "duplicate enemy id '%s' (also in %s)", meta.id.c_str(),
                          it->second.c_str());
            ok = false;
          }
        }
        if (!meta.display.empty()) {
          auto [it, inserted] = enemyDisplayToPath.emplace(meta.display, p);
          if (!inserted && it->second != p) {
            validateWarn(st, p, "duplicate enemy display '%s' (also in %s)", meta.display.c_str(),
                         it->second.c_str());
          }
        }
      }

      const std::string scriptDir = Paths::resolveAssetPath("assets/input_scripts", argv[0]);
      for (const auto& p : Paths::listTomlFiles(scriptDir))
        ok = validateInputScriptToml(st, p) && ok;

      const std::string formDir = Paths::resolveAssetPath("assets/forms", argv[0]);
      const std::vector<std::string> forms = Paths::listTomlFiles(formDir);
      if (forms.empty()) {
        validateError(st, formDir, "no form TOMLs found");
        ok = false;
      }
      for (const auto& p : forms) {
        FormMeta meta{};
        ok = validateFormToml(st, p, &meta) && ok;
        if (!meta.id.empty()) {
          auto [it, inserted] = formIdToPath.emplace(meta.id, p);
          if (!inserted && it->second != p) {
            validateError(st, p, "duplicate form id '%s' (also in %s)", meta.id.c_str(),
                          it->second.c_str());
            ok = false;
          }
        }
      }

      validateFallbackTestStageParity(st, argv[0]);
    }

    const int parseWarnings = TomlUtil::warningCount();
    const int totalWarnings = st.warnings + parseWarnings;
    std::printf(
        "validated: stages=%d characters=%d enemies=%d forms=%d scripts=%d  errors=%d warnings=%d "
        "parse_warnings=%d\n",
        st.stageFiles, st.characterFiles, st.enemyFiles, st.formFiles, st.inputScriptFiles,
        st.errors, st.warnings, parseWarnings);
    const bool strictOk = ok && (st.errors == 0) && (totalWarnings == 0);
    return (strict ? strictOk : ok) ? 0 : 1;
  }

  if (videoDriver) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, videoDriver);
  }

  App app;
  if (!app.init(cfg))
    return 1;
  app.run(maxFrames);
  App::PlayerSnapshot snap{};
  (void)app.playerSnapshot(snap);
  const std::string stagePath = app.stagePath();
  const std::string characterPath = app.characterPath();
  const std::string spawnPoint = app.spawnPoint();
  const float spawnX = app.spawnX();
  const float spawnY = app.spawnY();
  const std::string inputScriptPath = app.hasInputScript() ? app.inputScriptPath() : "";
  app.shutdown();

  auto dumpContext = [&]() {
    std::fprintf(stderr, "smoke context:\n");
    std::fprintf(stderr, "  stage: %s\n", stagePath.c_str());
    std::fprintf(stderr, "  character: %s\n", characterPath.c_str());
    if (!spawnPoint.empty())
      std::fprintf(stderr, "  spawn_point: %s\n", spawnPoint.c_str());
    else
      std::fprintf(stderr, "  spawn: %.1f %.1f\n", spawnX, spawnY);
    if (!inputScriptPath.empty())
      std::fprintf(stderr, "  input_script: %s\n", inputScriptPath.c_str());
    if (snap.valid) {
      std::fprintf(stderr,
                   "  player: pos=(%.2f,%.2f) vel=(%.2f,%.2f) grounded=%d respawns=%d "
                   "hurt_events=%d enemy_kills=%d  camera: (%.2f,%.2f) camera_backscrolled=%d\n",
                   snap.x, snap.y, snap.vx, snap.vy, snap.grounded ? 1 : 0, snap.respawns,
                   snap.hurtEvents, snap.enemyKills, snap.camX, snap.camY,
                   snap.cameraBackscrolled ? 1 : 0);
    } else {
      std::fprintf(stderr, "  player: <missing>\n");
    }
  };

  if (expectGrounded && !snap.grounded) {
    std::fprintf(stderr, "expected grounded at exit\n");
    dumpContext();
    return 1;
  }
  if (expectNoRespawn && snap.respawns != 0) {
    std::fprintf(stderr, "expected no respawns (got %d)\n", snap.respawns);
    dumpContext();
    return 1;
  }
  if (expectMinX) {
    if (!snap.valid) {
      std::fprintf(stderr, "expected player x >= %.2f at exit (player missing)\n", expectedMinX);
      dumpContext();
      return 1;
    }
    if (snap.x < expectedMinX) {
      std::fprintf(stderr, "expected player x >= %.2f at exit (got %.2f)\n", expectedMinX, snap.x);
      dumpContext();
      return 1;
    }
  }
  if (expectMaxX) {
    if (!snap.valid) {
      std::fprintf(stderr, "expected player x <= %.2f at exit (player missing)\n", expectedMaxX);
      dumpContext();
      return 1;
    }
    if (snap.x > expectedMaxX) {
      std::fprintf(stderr, "expected player x <= %.2f at exit (got %.2f)\n", expectedMaxX, snap.x);
      dumpContext();
      return 1;
    }
  }
  if (expectMinVx) {
    if (!snap.valid) {
      std::fprintf(stderr, "expected player vx >= %.2f at exit (player missing)\n", expectedMinVx);
      dumpContext();
      return 1;
    }
    if (snap.vx < expectedMinVx) {
      std::fprintf(stderr, "expected player vx >= %.2f at exit (got %.2f)\n", expectedMinVx,
                   snap.vx);
      dumpContext();
      return 1;
    }
  }
  if (expectMaxVx) {
    if (!snap.valid) {
      std::fprintf(stderr, "expected player vx <= %.2f at exit (player missing)\n", expectedMaxVx);
      dumpContext();
      return 1;
    }
    if (snap.vx > expectedMaxVx) {
      std::fprintf(stderr, "expected player vx <= %.2f at exit (got %.2f)\n", expectedMaxVx,
                   snap.vx);
      dumpContext();
      return 1;
    }
  }
  if (expectMinY) {
    if (!snap.valid) {
      std::fprintf(stderr, "expected player y >= %.2f at exit (player missing)\n", expectedMinY);
      dumpContext();
      return 1;
    }
    if (snap.y < expectedMinY) {
      std::fprintf(stderr, "expected player y >= %.2f at exit (got %.2f)\n", expectedMinY, snap.y);
      dumpContext();
      return 1;
    }
  }
  if (expectMaxY) {
    if (!snap.valid) {
      std::fprintf(stderr, "expected player y <= %.2f at exit (player missing)\n", expectedMaxY);
      dumpContext();
      return 1;
    }
    if (snap.y > expectedMaxY) {
      std::fprintf(stderr, "expected player y <= %.2f at exit (got %.2f)\n", expectedMaxY, snap.y);
      dumpContext();
      return 1;
    }
  }
  if (expectMinCamX) {
    if (!snap.valid) {
      std::fprintf(stderr, "expected camera x >= %.2f at exit (player missing)\n", expectedMinCamX);
      dumpContext();
      return 1;
    }
    if (snap.camX < expectedMinCamX) {
      std::fprintf(stderr, "expected camera x >= %.2f at exit (got %.2f)\n", expectedMinCamX,
                   snap.camX);
      dumpContext();
      return 1;
    }
  }
  if (expectMaxCamX) {
    if (!snap.valid) {
      std::fprintf(stderr, "expected camera x <= %.2f at exit (player missing)\n", expectedMaxCamX);
      dumpContext();
      return 1;
    }
    if (snap.camX > expectedMaxCamX) {
      std::fprintf(stderr, "expected camera x <= %.2f at exit (got %.2f)\n", expectedMaxCamX,
                   snap.camX);
      dumpContext();
      return 1;
    }
  }
  if (expectMinCamY) {
    if (!snap.valid) {
      std::fprintf(stderr, "expected camera y >= %.2f at exit (player missing)\n", expectedMinCamY);
      dumpContext();
      return 1;
    }
    if (snap.camY < expectedMinCamY) {
      std::fprintf(stderr, "expected camera y >= %.2f at exit (got %.2f)\n", expectedMinCamY,
                   snap.camY);
      dumpContext();
      return 1;
    }
  }
  if (expectMaxCamY) {
    if (!snap.valid) {
      std::fprintf(stderr, "expected camera y <= %.2f at exit (player missing)\n", expectedMaxCamY);
      dumpContext();
      return 1;
    }
    if (snap.camY > expectedMaxCamY) {
      std::fprintf(stderr, "expected camera y <= %.2f at exit (got %.2f)\n", expectedMaxCamY,
                   snap.camY);
      dumpContext();
      return 1;
    }
  }
  if (expectNoCameraBackscroll && snap.cameraBackscrolled) {
    std::fprintf(stderr, "expected no camera backscroll\n");
    dumpContext();
    return 1;
  }
  if (expectHurtCount && snap.hurtEvents != expectedHurtCount) {
    std::fprintf(stderr, "expected hurt_events == %d (got %d)\n", expectedHurtCount,
                 snap.hurtEvents);
    dumpContext();
    return 1;
  }
  if (expectEnemyKills && snap.enemyKills != expectedEnemyKills) {
    std::fprintf(stderr, "expected enemy_kills == %d (got %d)\n", expectedEnemyKills,
                 snap.enemyKills);
    dumpContext();
    return 1;
  }
  return 0;
}
