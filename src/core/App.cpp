#include "core/App.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <format>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#include <SDL3/SDL_blendmode.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>

#include "character/Actions.h"
#include "character/CharacterConfig.h"
#include "character/CharacterController.h"
#include "collectible/CollectibleConfig.h"
#include "core/Prefs.h"
#include "ecs/Components.h"
#include "ecs/Entity.h"
#include "enemy/EnemyConfig.h"
#include "util/Paths.h"
#include "visual/CharacterForms.h"
#include "visual/Palette.h"
#include "visual/Rules.h"

static constexpr int kW = 960;
static constexpr int kH = 540;
static constexpr float kCameraLookStillSpeed = 5.0F;  // px/s (while grounded)
static constexpr float kTimeScaleMin = 0.1F;
static constexpr float kTimeScaleMax = 4.0F;
static constexpr int kGamepadDeadzoneMin = 0;
static constexpr int kGamepadDeadzoneMax = 32767;

static void chooseInternalResolution(int availW, int availH, int& outW, int& outH) {
  struct Mode {
    int w;
    int h;
  };

  static constexpr std::array<Mode, 4> kModes = {{
      {960, 540},
      {640, 360},
      {480, 270},
      {320, 180},
  }};

  outW = kModes[0].w;  // default: a comfortable, wide view
  outH = kModes[0].h;

  if (availW <= 0 || availH <= 0) {
    return;
  }

  // Auto policy: prefer a wide internal view (less “zoomed in”), but avoid rendering so large that
  // the docked Game panel has to downscale it heavily (which makes the game hard to read).
  // NOTE: In the default dock layout, the Game panel is narrower than the SDL window; allow a
  // fairly aggressive downscale before switching to a smaller internal resolution so the camera
  // view doesn't feel "zoomed in" when UI panels are open.
  static constexpr float kMinAutoScale = 0.45F;

  for (const Mode& m : kModes) {
    const float scaleX = static_cast<float>(availW) / static_cast<float>(m.w);
    const float scaleY = static_cast<float>(availH) / static_cast<float>(m.h);
    const float scale = std::min(scaleX, scaleY);
    if (scale >= kMinAutoScale) {
      outW = m.w;
      outH = m.h;
      return;
    }
  }

  // If the panel is very small, fall back to the smallest mode.
  outW = kModes.back().w;
  outH = kModes.back().h;
}

static std::string rgbHex(const Rgba8& c) {
  return std::format("{:02X}{:02X}{:02X}", static_cast<unsigned int>(c.r),
                     static_cast<unsigned int>(c.g), static_cast<unsigned int>(c.b));
}

static Visual::Color enemyColorForType(EnemyConfig::Type type) {
  switch (type) {
    case EnemyConfig::Type::Walker:
      return Visual::Color{240, 170, 60, 255};
    case EnemyConfig::Type::Spiky:
      return Visual::Color{230, 70, 70, 255};
    case EnemyConfig::Type::Shooter:
      return Visual::Color{140, 90, 220, 255};
    case EnemyConfig::Type::Hopper:
      return Visual::Color{90, 200, 120, 255};
  }
  return Visual::Color{220, 220, 220, 255};
}

static Visual::Color projectileColor(bool fromEnemy) {
  if (fromEnemy) {
    return Visual::Color{255, 120, 60, 255};
  }
  return Visual::Color{90, 230, 255, 255};
}

static Visual::Color projectileGlowColor(bool fromEnemy) {
  if (fromEnemy) {
    return Visual::Color{255, 90, 40, 140};
  }
  return Visual::Color{80, 210, 255, 150};
}

static Visual::Color hitboxFillColor(bool fromPlayer, bool active) {
  if (fromPlayer) {
    return active ? Visual::Color{80, 220, 255, 90} : Visual::Color{80, 220, 255, 50};
  }
  return active ? Visual::Color{255, 120, 90, 90} : Visual::Color{255, 120, 90, 50};
}

static Visual::Color hitboxOutlineColor(bool fromPlayer, bool active) {
  if (fromPlayer) {
    return active ? Visual::Color{120, 240, 255, 220} : Visual::Color{120, 240, 255, 140};
  }
  return active ? Visual::Color{255, 150, 110, 220} : Visual::Color{255, 150, 110, 140};
}

static Visual::FormStyle resolveFormStyle(const Visual::CharacterForm& form,
                                          const CharacterConfig* cfg) {
  Visual::FormStyle style{};
  style.palette = form.palette;
  if (cfg == nullptr) {
    return style;
  }

  const CharacterConfig::FormStyle& formCfg = cfg->form;
  if (!formCfg.paletteBase.empty() || !formCfg.paletteAccent.empty()) {
    const Visual::Color base = formCfg.paletteBase.empty()
                                   ? form.palette.base
                                   : Visual::Color::fromHex(formCfg.paletteBase);
    const Visual::Color accent = formCfg.paletteAccent.empty()
                                     ? form.palette.accent
                                     : Visual::Color::fromHex(formCfg.paletteAccent);
    style.palette = Visual::buildPalette(base, accent, Visual::gRules);
  }

  style.namedColors.reserve(formCfg.colors.size());
  for (const auto& [key, value] : formCfg.colors) {
    style.namedColors[key] = Visual::Color::fromHex(value);
  }

  style.variants = formCfg.variants;
  return style;
}

bool App::init(const AppConfig& cfg) {
  renderEnabled_ = !cfg.noRender;
  uiEnabled_ = renderEnabled_ && !cfg.noUi;
  prefsEnabled_ = !cfg.noPrefs;

  const SDL_InitFlags initFlags =
      renderEnabled_
          ? static_cast<SDL_InitFlags>(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD)
          : static_cast<SDL_InitFlags>(SDL_INIT_EVENTS | SDL_INIT_GAMEPAD);
  if (!SDL_Init(initFlags)) {
    std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
    return false;
  }

  input_.init();

  if (cfg.inputScriptTomlPath != nullptr) {
    const std::string scriptPath = Paths::resolveAssetPath(cfg.inputScriptTomlPath);
    if (!inputScript_.loadFromToml(scriptPath.c_str())) {
      std::cerr << "Failed to load input script TOML: " << scriptPath << '\n';
      return false;
    }
    inputScriptEnabled_ = true;
  }

  SessionPrefs prefs{};
  if (prefsEnabled_) {
    (void)loadSessionPrefs(prefs);
  }

  if (cfg.stageTomlPath != nullptr) {
    prefs.stagePath = cfg.stageTomlPath;
  }
  if (cfg.characterTomlPath != nullptr) {
    prefs.characterPath = cfg.characterTomlPath;
  }

  if (cfg.spawnPoint != nullptr) {
    prefs.spawnPoint = cfg.spawnPoint;
  }
  if (cfg.spawnOverride) {
    prefs.spawnPoint.clear();
    prefs.spawnX = cfg.spawnX;
    prefs.spawnY = cfg.spawnY;
  }

  panelsOpen_ = prefs.panelsOpen;
  timeScale_ = std::clamp(prefs.timeScale, kTimeScaleMin, kTimeScaleMax);
  autoReload_ = prefs.autoReload;
  internalResMode_ = std::clamp(prefs.internalResMode, 0, 4);
  integerScaleOnly_ = prefs.integerScaleOnly;
  gamepadDeadzone_ = std::clamp(prefs.gamepadDeadzone, kGamepadDeadzoneMin, kGamepadDeadzoneMax);
  input_.setGamepadDeadzone(gamepadDeadzone_);

  if (renderEnabled_) {
    window_ = SDL_CreateWindow("SDL3 Sandbox", kW, kH, SDL_WINDOW_RESIZABLE);
    if (window_ == nullptr) {
      std::cerr << "CreateWindow failed: " << SDL_GetError() << '\n';
      return false;
    }

    renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (renderer_ == nullptr) {
      std::cerr << "CreateRenderer failed: " << SDL_GetError() << '\n';
      return false;
    }
    SDL_SetRenderVSync(renderer_, 1);

    cursorDefault_ = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
    cursorNwResize_ = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NW_RESIZE);
    cursorNeResize_ = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NE_RESIZE);
    cursorSeResize_ = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SE_RESIZE);
    cursorSwResize_ = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SW_RESIZE);

    sprites_.init(renderer_);
    if (uiEnabled_) {
      (void)debugUi_.init(window_, renderer_);
    }

    // Initialize visual system
    shapeRenderer_ = std::make_unique<Visual::ShapeRenderer>(renderer_);
    formRenderer_ = std::make_unique<Visual::FormRenderer>(*shapeRenderer_, Visual::gRules);
    formPath_ = Paths::resolveAssetPath("assets/forms/puppet.toml");
    rebuildCharacterForms();
  }

  std::string stagePath =
      prefs.stagePath.empty() ? "assets/stages/skyway_run.toml" : prefs.stagePath;
  stagePath_ = Paths::resolveAssetPath(stagePath);
  if (!stage_.loadFromToml(stagePath_.c_str())) {
    std::cerr << "Failed to load stage TOML: " << stagePath_ << '\n';
    stage_.loadTestStage();
  }

  spawnPoint_ = prefs.spawnPoint;
  spawnX_ = prefs.spawnX;
  spawnY_ = prefs.spawnY;
  int spawnFacing = 1;
  if (!spawnPoint_.empty()) {
    float sx = spawnX_;
    float sy = spawnY_;
    int facing = spawnFacing;
    if (stage_.getSpawn(spawnPoint_.c_str(), sx, sy, facing)) {
      spawnX_ = sx;
      spawnY_ = sy;
      spawnFacing = facing;
    } else {
      std::cerr << "Unknown spawn point: " << spawnPoint_ << '\n';
      spawnPoint_.clear();
    }
  }

  std::string characterPath =
      prefs.characterPath.empty() ? "assets/characters/bolt.toml" : prefs.characterPath;
  characterPath_ = Paths::resolveAssetPath(characterPath);
  Stage::spawnDemo(world_, characterPath_.c_str(), spawnX_, spawnY_, spawnFacing);
  respawnEnemies();
  respawnCollectibles();

  refreshMenuLists();
  syncWatchedFiles();

  return true;
}

void App::rebuildCharacterForms() {
  puppetForm_ = Visual::createPuppetForm();
  fallbackForm_ = Visual::createFallbackForm();
  formRuntime_.clear();
}

void App::shutdown() {
  if (prefsEnabled_) {
    SessionPrefs prefs{};
    prefs.stagePath = stagePath_;
    prefs.characterPath = characterPath_;
    prefs.spawnPoint = spawnPoint_;
    prefs.spawnX = spawnX_;
    prefs.spawnY = spawnY_;
    prefs.panelsOpen = panelsOpen_;
    prefs.autoReload = autoReload_;
    prefs.timeScale = timeScale_;
    prefs.internalResMode = internalResMode_;
    prefs.integerScaleOnly = integerScaleOnly_;
    prefs.gamepadDeadzone = gamepadDeadzone_;
    (void)saveSessionPrefs(prefs);
  }

  debugUi_.shutdown();
  sprites_.shutdown();
  input_.shutdown();
  if (cursorDefault_ != nullptr) {
    SDL_DestroyCursor(cursorDefault_);
    cursorDefault_ = nullptr;
  }
  if (cursorNwResize_ != nullptr) {
    SDL_DestroyCursor(cursorNwResize_);
    cursorNwResize_ = nullptr;
  }
  if (cursorNeResize_ != nullptr) {
    SDL_DestroyCursor(cursorNeResize_);
    cursorNeResize_ = nullptr;
  }
  if (cursorSeResize_ != nullptr) {
    SDL_DestroyCursor(cursorSeResize_);
    cursorSeResize_ = nullptr;
  }
  if (cursorSwResize_ != nullptr) {
    SDL_DestroyCursor(cursorSwResize_);
    cursorSwResize_ = nullptr;
  }
  if (gameTarget_ != nullptr) {
    SDL_DestroyTexture(gameTarget_);
    gameTarget_ = nullptr;
  }
  if (renderer_ != nullptr) {
    SDL_DestroyRenderer(renderer_);
  }
  if (window_ != nullptr) {
    SDL_DestroyWindow(window_);
  }
  SDL_Quit();
}

bool App::playerGrounded() const {
  if (world_.player == kInvalidEntity) {
    return false;
  }
  auto* groundedPtr = world_.registry.try_get<Grounded>(world_.player);
  if (groundedPtr == nullptr) {
    return false;
  }
  return groundedPtr->onGround;
}

int App::respawnCount() const {
  return respawnCount_;
}

bool App::playerSnapshot(PlayerSnapshot& out) const {
  out = PlayerSnapshot{};
  out.respawns = respawnCount_;
  out.hurtEvents = world_.hurtEvents;
  out.enemyKills = world_.enemyKills;
  out.camX = camX_;
  out.camY = camY_;
  out.cameraBackscrolled = cameraBackscrolled_;

  if (world_.player == kInvalidEntity) {
    return false;
  }

  const EntityId id = world_.player;
  auto* trPtr = world_.registry.try_get<Transform>(id);
  if (trPtr == nullptr) {
    return false;
  }
  out.x = trPtr->pos.x;
  out.y = trPtr->pos.y;

  auto* velPtr = world_.registry.try_get<Velocity>(id);
  if (velPtr != nullptr) {
    out.vx = velPtr->v.x;
    out.vy = velPtr->v.y;
  }

  out.grounded = playerGrounded();
  out.valid = true;
  return true;
}

void App::handleEvent(const SDL_Event& e) {
  if (e.type == SDL_EVENT_QUIT) {
    running_ = false;
  }
}

void App::handleCommands(const AppCommands& cmds) {
  if (cmds.quit) {
    running_ = false;
  }
  if (cmds.toggleDebugOverlay) {
    debugOverlay_ = !debugOverlay_;
  }
  if (cmds.toggleDebugCollision) {
    debugCollision_ = !debugCollision_;
  }
  if (cmds.togglePanels) {
    panelsOpen_ = !panelsOpen_;
    if (panelsOpen_) {
      refreshMenuLists();
    }
  }

  if (cmds.respawn) {
    respawnPlayer();
  }
  if (cmds.resetState) {
    resetPlayerState();
  }
}

void App::updateCameraLook(TimeStep ts) {
  auto resetCameraLook = [&]() {
    cameraLookOffsetY_ = 0.0F;
    lookUpHeldMs_ = 0;
    lookDownHeldMs_ = 0;
  };

  // Camera look-up/look-down is purely a camera behavior (does not affect simulation).
  // We update it per sim tick so headless runs match interactive runs.
  if (world_.player == kInvalidEntity) {
    resetCameraLook();
    return;
  }

  const int holdMs = stage_.cameraLookHoldMs();
  const float lookUpY = stage_.cameraLookUpY();
  const float lookDownY = stage_.cameraLookDownY();
  if (holdMs <= 0 || (lookUpY <= 0.0F && lookDownY <= 0.0F)) {
    resetCameraLook();
    return;
  }

  auto* inputPtr = world_.registry.try_get<InputState>(world_.player);
  auto* groundedPtr = world_.registry.try_get<Grounded>(world_.player);
  auto* velPtr = world_.registry.try_get<Velocity>(world_.player);
  if (inputPtr == nullptr || groundedPtr == nullptr || velPtr == nullptr) {
    resetCameraLook();
    return;
  }

  const InputState& in = *inputPtr;
  const bool still = groundedPtr->onGround && std::fabs(velPtr->v.x) < kCameraLookStillSpeed;
  if (!still) {
    resetCameraLook();
    return;
  }

  const int dtms = static_cast<int>(std::round(ts.dt * 1000.0F));
  const bool up = in.upHeld && !in.downHeld;
  const bool down = in.downHeld && !in.upHeld;
  if (up) {
    lookUpHeldMs_ += dtms;
    lookDownHeldMs_ = 0;
  } else if (down) {
    lookDownHeldMs_ += dtms;
    lookUpHeldMs_ = 0;
  } else {
    lookUpHeldMs_ = 0;
    lookDownHeldMs_ = 0;
  }

  float target = 0.0F;
  if (up && lookUpY > 0.0F && lookUpHeldMs_ >= holdMs) {
    target = -lookUpY;
  }
  if (down && lookDownY > 0.0F && lookDownHeldMs_ >= holdMs) {
    target = lookDownY;
  }
  cameraLookOffsetY_ = target;
}

void App::applyKillPlane() {
  // Kill plane: if we have world bounds and the player falls far below the world, respawn.
  // This is a sandbox QoL feature (keeps iteration tight) and remains deterministic.
  if (world_.player == kInvalidEntity) {
    return;
  }

  Rect worldBounds{};
  if (!stage_.getWorldBounds(worldBounds)) {
    return;
  }

  auto* trPtr = world_.registry.try_get<Transform>(world_.player);
  auto* boxPtr = world_.registry.try_get<AABB>(world_.player);
  if (trPtr == nullptr || boxPtr == nullptr) {
    return;
  }

  static constexpr float kKillPlaneMargin = 256.0F;
  const float bottom = trPtr->pos.y + boxPtr->h;
  const float killY = worldBounds.y + worldBounds.h + kKillPlaneMargin;
  if (bottom > killY) {
    respawnPlayer();
  }
}

void App::resetCameraTracking() {
  camX_ = 0.0F;
  camY_ = 0.0F;
  cameraSnapPending_ = true;
  cameraTrackValid_ = false;
}

int App::resolveCameraFacing(EntityId id) const {
  auto* asPtr = world_.registry.try_get<ActionState>(id);
  if (asPtr != nullptr) {
    return (asPtr->facingX < 0) ? -1 : 1;
  }

  auto* velPtr = world_.registry.try_get<Velocity>(id);
  if (velPtr != nullptr) {
    if (velPtr->v.x > 0.0F) {
      return 1;
    }
    if (velPtr->v.x < 0.0F) {
      return -1;
    }
  }

  return 1;
}

bool App::resolveCameraTarget(float& targetX,
                              float& targetY,
                              Rect& playerAabb,
                              float& boxW,
                              float& boxH) {
  if (world_.player == kInvalidEntity) {
    resetCameraTracking();
    return false;
  }

  auto* trPtr = world_.registry.try_get<Transform>(world_.player);
  if (trPtr == nullptr) {
    resetCameraTracking();
    return false;
  }

  boxW = 0.0F;
  boxH = 0.0F;
  auto* boxPtr = world_.registry.try_get<AABB>(world_.player);
  if (boxPtr != nullptr) {
    boxW = boxPtr->w;
    boxH = boxPtr->h;
  }

  playerAabb = Rect{trPtr->pos.x, trPtr->pos.y, boxW, boxH};

  const float centerX = trPtr->pos.x + boxW * 0.5F;
  const float centerY = trPtr->pos.y + boxH * 0.5F;
  const int facing = resolveCameraFacing(world_.player);
  const float lookaheadX = stage_.cameraLookaheadX() * static_cast<float>(facing);
  const float lookaheadY = stage_.cameraLookaheadY();
  targetX = centerX + lookaheadX;
  targetY = centerY + lookaheadY + cameraLookOffsetY_;
  return true;
}

void App::applyCameraDeadzone(bool shouldSnap,
                              float targetX,
                              float targetY,
                              float viewHalfW,
                              float viewHalfH,
                              float& desiredX,
                              float& desiredY) {
  if (shouldSnap) {
    return;
  }

  const float dzW = stage_.cameraDeadzoneW();
  const float dzH = stage_.cameraDeadzoneH();
  if (dzW <= 0.0F && dzH <= 0.0F) {
    return;
  }

  float camCenterX = camX_ + viewHalfW;
  float camCenterY = camY_ + viewHalfH;

  static constexpr float kDeadzoneHalf = 0.5F;
  float halfDzW = std::min(std::max(0.0F, dzW) * kDeadzoneHalf, viewHalfW);
  float halfDzH = std::min(std::max(0.0F, dzH) * kDeadzoneHalf, viewHalfH);

  const float dx = targetX - camCenterX;
  if (dx > halfDzW) {
    camCenterX += dx - halfDzW;
  } else if (dx < -halfDzW) {
    camCenterX += dx + halfDzW;
  }

  const float dy = targetY - camCenterY;
  if (dy > halfDzH) {
    camCenterY += dy - halfDzH;
  } else if (dy < -halfDzH) {
    camCenterY += dy + halfDzH;
  }

  desiredX = camCenterX - viewHalfW;
  desiredY = camCenterY - viewHalfH;
}

void App::tick(TimeStep ts) {
  lastTs_ = ts;
  world_.update(stage_, ts);
  updateCameraLook(ts);
  applyKillPlane();
  if (characterSwapZapTimer_ > 0.0F) {
    characterSwapZapTimer_ = std::max(0.0F, characterSwapZapTimer_ - ts.dt);
  }

  // Keep camera state updated even in headless/no-render mode (useful for deterministic smokes).
  if (!renderEnabled_) {
    updateCamera(kW, kH);
  }
}

static float clampf(float v, float lo, float hi) {
  if (hi < lo) {
    hi = lo;
  }
  return std::clamp(v, lo, hi);
}

void App::updateCamera(int viewW, int viewH) {
  if (viewW <= 0) {
    viewW = kW;
  }
  if (viewH <= 0) {
    viewH = kH;
  }

  const float prevCamX = camX_;
  float targetX = 0.0F;
  float targetY = 0.0F;
  float boxW = 0.0F;
  float boxH = 0.0F;
  Rect playerAabb{};
  if (!resolveCameraTarget(targetX, targetY, playerAabb, boxW, boxH)) {
    return;
  }

  const float viewHalfW = static_cast<float>(viewW) * 0.5F;
  const float viewHalfH = static_cast<float>(viewH) * 0.5F;

  float desiredX = targetX - viewHalfW;
  float desiredY = targetY - viewHalfH;

  // When snapping (initial load or respawn), skip deadzone and go directly to target
  const bool shouldSnap = cameraSnapPending_;
  cameraSnapPending_ = false;
  if (shouldSnap) {
    cameraTrackValid_ = false;
  }

  applyCameraDeadzone(shouldSnap, targetX, targetY, viewHalfW, viewHalfH, desiredX, desiredY);

  Rect lockBounds{};
  const bool hasLock =
      (boxW > 0.0F && boxH > 0.0F) && stage_.cameraLockBoundsAt(playerAabb, lockBounds);

  if (!shouldSnap && stage_.cameraNoBackscroll() && !hasLock) {
    desiredX = std::max(desiredX, prevCamX);
  }

  Rect bounds{};
  bool strictBounds = false;
  if (hasLock) {
    bounds = lockBounds;
    strictBounds = true;
  }

  const bool hasBounds = hasLock || stage_.getCameraBounds(bounds) || stage_.getWorldBounds(bounds);
  if (hasBounds) {
    float minX = bounds.x;
    float minY = bounds.y;
    float maxX = bounds.x + bounds.w - static_cast<float>(viewW);
    float maxY = bounds.y + bounds.h - static_cast<float>(viewH);

    if (!strictBounds) {
      const float targetMargin = 64.0F;
      minX = std::min(minX, targetX - viewHalfW + targetMargin);
      minY = std::min(minY, targetY - viewHalfH + targetMargin);
      maxX = std::max(maxX, targetX - viewHalfW - targetMargin);
      maxY = std::max(maxY, targetY - viewHalfH - targetMargin);
    }

    camX_ = clampf(desiredX, minX, maxX);
    camY_ = clampf(desiredY, minY, maxY);
  } else {
    camX_ = desiredX;
    camY_ = desiredY;
  }

  static constexpr float kBackscrollEpsilon = 0.001F;
  if (!shouldSnap && cameraTrackValid_ && camX_ < lastTrackedCamX_ - kBackscrollEpsilon) {
    cameraBackscrolled_ = true;
  }
  lastTrackedCamX_ = camX_;
  cameraTrackValid_ = true;
}

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
// NOLINTNEXTLINE
void App::render() {
  if (!renderEnabled_ || (renderer_ == nullptr)) {
    return;
  }

  int winW = kW;
  int winH = kH;
  (void)SDL_GetWindowSize(window_, &winW, &winH);

  auto ensureGameTarget = [&](int w, int h) {
    if (w <= 0 || h <= 0) {
      return;
    }
    if ((gameTarget_ != nullptr) && gameTargetW_ == w && gameTargetH_ == h) {
      return;
    }

    if (gameTarget_ != nullptr) {
      SDL_DestroyTexture(gameTarget_);
    }
    gameTarget_ = nullptr;
    gameTargetW_ = 0;
    gameTargetH_ = 0;

    SDL_Texture* tex =
        SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, w, h);
    if (tex == nullptr) {
      std::cerr << "SDL_CreateTexture (render target) failed: " << SDL_GetError() << '\n';
      return;
    }

    (void)SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
    (void)SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

    gameTarget_ = tex;
    gameTargetW_ = w;
    gameTargetH_ = h;
  };

  SDL_SetRenderViewport(renderer_, nullptr);
  SDL_SetRenderDrawColor(renderer_, 12, 12, 16, 255);
  SDL_RenderClear(renderer_);

  if (debugUi_.initialized()) {
    debugUi_.beginFrame();
  }

  int internalW = kW;
  int internalH = kH;

  // NOLINTNEXTLINE
  auto applyMenuActions = [&](const DebugUIMenuActions& actions) {
    bool refreshWatchedFiles = false;

    if (actions.resetLayoutAndPrefs) {
      (void)deleteSessionPrefs();
      (void)deleteImGuiIni();

      panelsOpen_ = true;
      simPaused_ = false;
      pendingSimSteps_ = 0;
      timeScale_ = 1.0F;
      internalResMode_ = 0;
      integerScaleOnly_ = false;
      gamepadDeadzone_ = 8000;
      input_.setGamepadDeadzone(gamepadDeadzone_);
      autoReload_ = false;
      Visual::gRules = Visual::kDefaultRules;
      rebuildCharacterForms();

      const std::string defaultStage = Paths::resolveAssetPath("assets/stages/skyway_run.toml");
      if (!stage_.loadFromToml(defaultStage.c_str())) {
        std::cerr << "Failed to load stage TOML: " << defaultStage << '\n';
        stage_.loadTestStage();
      }
      stagePath_ = defaultStage;
      refreshSpawnLists();
      spawnPoint_ = "default";
      spawnX_ = 120.0F;
      spawnY_ = 120.0F;

      characterPath_ = Paths::resolveAssetPath("assets/characters/bolt.toml");
      respawnPlayer();

      refreshMenuLists();
      syncWatchedFiles();
    }

    if (actions.quit) {
      running_ = false;
    }

    if (actions.resetState) {
      resetPlayerState();
    }

    if (actions.setSimPaused) {
      simPaused_ = actions.simPaused;
      if (!simPaused_) {
        pendingSimSteps_ = 0;
      }
    }

    if (actions.stepFrames > 0) {
      pendingSimSteps_ += actions.stepFrames;
      simPaused_ = true;
    }

    if (actions.setTimeScale) {
      timeScale_ = std::clamp(actions.timeScale, 0.1F, 4.0F);
    }

    if (actions.setInternalResMode) {
      internalResMode_ = std::clamp(actions.internalResMode, 0, 4);
    }

    if (actions.setIntegerScaleOnly) {
      integerScaleOnly_ = actions.integerScaleOnly;
    }

    if (actions.setGamepadDeadzone) {
      gamepadDeadzone_ = std::clamp(actions.gamepadDeadzone, 0, 32767);
      input_.setGamepadDeadzone(gamepadDeadzone_);
    }

    if (actions.resetVisualRules) {
      Visual::gRules = Visual::kDefaultRules;
      rebuildCharacterForms();
    }

    if (actions.setVisualRules) {
      const bool paletteChanged =
          (Visual::gRules.highlightWarm != actions.visualRules.highlightWarm ||
           Visual::gRules.shadowCool != actions.visualRules.shadowCool);

      Visual::gRules.highlightWarm = actions.visualRules.highlightWarm;
      Visual::gRules.shadowCool = actions.visualRules.shadowCool;
      Visual::gRules.outlineScale = std::clamp(actions.visualRules.outlineScale, 1.0F, 1.5F);
      Visual::gRules.squashStretchMax =
          std::clamp(actions.visualRules.squashStretchMax, 1.0F, 3.0F);
      Visual::gRules.preserveMass = actions.visualRules.preserveMass;
      Visual::gRules.smearThreshold = std::clamp(actions.visualRules.smearThreshold, 0.0F, 5000.0F);
      Visual::gRules.afterimageCount =
          std::clamp(actions.visualRules.afterimageCount, 0, kMaxAfterimages);
      Visual::gRules.afterimageDecay =
          std::clamp(actions.visualRules.afterimageDecay, 0.01F, 10.0F);

      if (paletteChanged) {
        rebuildCharacterForms();
      }
    }

    if (actions.setAutoReload) {
      autoReload_ = actions.autoReload;
      refreshWatchedFiles = true;
    }

    if (actions.reloadStage) {
      if (reloadStageFromDisk()) {
        respawnPlayer();
      }
      refreshWatchedFiles = true;
    }

    if (actions.reloadCharacterInPlace) {
      (void)reloadCharacterFromDiskInPlace(actions.reloadCharacterResetRuntime);
      refreshWatchedFiles = true;
    }

    if (actions.reloadCharacter) {
      respawnPlayer();
      refreshWatchedFiles = true;
    }

    if (actions.respawn) {
      respawnPlayer();
    }

    if (actions.teleportToSpawn) {
      teleportPlayerToSpawn();
    }

    if (actions.swapCharacterInPlace) {
      if (swapCharacterInPlace(actions.swapCharacterPath, true)) {
        refreshWatchedFiles = true;
      }
    }

    if (actions.selectCharacter) {
      characterPath_ = actions.characterPath;
      respawnPlayer();
      refreshWatchedFiles = true;
    }

    if (actions.selectStage) {
      const std::string nextPath = actions.stagePath;
      if (!stage_.loadFromToml(nextPath.c_str())) {
        std::cerr << "Failed to load stage TOML: " << nextPath << '\n';
      } else {
        stagePath_ = nextPath;
        refreshSpawnLists();
        spawnPoint_ = "default";
        respawnPlayer();
      }
      refreshWatchedFiles = true;
    }

    if (actions.selectSpawnPoint) {
      spawnPoint_ = actions.spawnPoint;
      respawnPlayer();
    }

    if (refreshWatchedFiles) {
      syncWatchedFiles();
    }
  };

  auto makeOverlayModel = [&]() {
    DebugUIOverlayModel out{};
    out.frame = lastTs_.frame;
    out.dt = lastTs_.dt;
    out.debugOverlay = debugOverlay_;
    out.debugCollision = debugCollision_;
    out.camX = camX_;
    out.camY = camY_;

    if (world_.player != kInvalidEntity) {
      const EntityId id = world_.player;
      out.hasPlayer = true;
      out.playerId = static_cast<uint32_t>(id);

      auto* namePtr = world_.registry.try_get<DebugName>(id);
      if (namePtr != nullptr) {
        out.playerName = namePtr->name;
      }

      auto* trPtr = world_.registry.try_get<Transform>(id);
      if (trPtr != nullptr) {
        out.hasPos = true;
        out.posX = trPtr->pos.x;
        out.posY = trPtr->pos.y;
      }

      auto* velPtr = world_.registry.try_get<Velocity>(id);
      if (velPtr != nullptr) {
        out.hasVel = true;
        out.velX = velPtr->v.x;
        out.velY = velPtr->v.y;
      }

      auto* boxPtr = world_.registry.try_get<AABB>(id);
      if (boxPtr != nullptr) {
        out.hasAabb = true;
        out.aabbW = boxPtr->w;
        out.aabbH = boxPtr->h;
      }

      auto* groundedPtr = world_.registry.try_get<Grounded>(id);
      if (groundedPtr != nullptr) {
        out.hasGrounded = true;
        out.grounded = groundedPtr->onGround;
      }

      auto* coyotePtr = world_.registry.try_get<CoyoteFrames>(id);
      auto* jbufPtr = world_.registry.try_get<JumpBufferFrames>(id);
      auto* acdPtr = world_.registry.try_get<ActionCooldownFrames>(id);
      auto* atkPtr = world_.registry.try_get<AttackCooldownFrames>(id);
      if (coyotePtr != nullptr && jbufPtr != nullptr && acdPtr != nullptr && atkPtr != nullptr) {
        out.hasTimers = true;
        out.coyoteFrames = coyotePtr->value;
        out.jumpBufferFrames = jbufPtr->value;
        out.actionCooldownFrames = acdPtr->value;
        out.attackCooldownFrames = atkPtr->value;
      }

      auto* asPtr = world_.registry.try_get<ActionState>(id);
      if (asPtr != nullptr) {
        out.hasActionState = true;
        out.dashRemainingFrames = asPtr->dashRemainingFrames;
        out.airDashesUsed = asPtr->airDashesUsed;
        out.spindashCharging = asPtr->spindashCharging;
        out.spindashChargeFrames = asPtr->spindashChargeFrames;
        out.spinning = asPtr->spinning;
        out.spinFacingX = asPtr->spinFacingX;
        out.flying = asPtr->flying;
        out.gliding = asPtr->gliding;
        out.walling = asPtr->walling;
        out.wallDirX = asPtr->wallDirX;
        out.dashWallKickFrames = asPtr->dashWallKickFrames;
        out.hasInput = true;
        out.dashHeld = asPtr->dashHeld;
      }
    }

    return out;
  };

  // NOLINTNEXTLINE
  auto makePlayerInspectorModel = [&]() {
    DebugUIPlayerInspectorModel playerModel{};
    playerModel.overlay = makeOverlayModel();
    playerModel.stageId = stage_.id();

    if (world_.player != kInvalidEntity) {
      const EntityId id = world_.player;

      auto* asPtr = world_.registry.try_get<ActionState>(id);
      if (asPtr != nullptr) {
        playerModel.hasFacing = true;
        playerModel.facingX = (asPtr->facingX < 0) ? -1 : 1;
      }

      auto* animPtr = world_.registry.try_get<AnimState>(world_.player);
      if (animPtr != nullptr) {
        switch (animPtr->id) {
          case AnimStateId::Idle:
            playerModel.animState = "Idle";
            break;
          case AnimStateId::Run:
            playerModel.animState = "Run";
            break;
          case AnimStateId::Jump:
            playerModel.animState = "Jump";
            break;
          case AnimStateId::Fall:
            playerModel.animState = "Fall";
            break;
          case AnimStateId::Dash:
            playerModel.animState = "Dash";
            break;
          case AnimStateId::Spin:
            playerModel.animState = "Spin";
            break;
          case AnimStateId::Fly:
            playerModel.animState = "Fly";
            break;
          case AnimStateId::Glide:
            playerModel.animState = "Glide";
            break;
          case AnimStateId::SpindashCharge:
            playerModel.animState = "SpindashCharge";
            break;
          case AnimStateId::AttackMelee:
            playerModel.animState = "AttackMelee";
            break;
          case AnimStateId::AttackShoot:
            playerModel.animState = "AttackShoot";
            break;
        }
      }

      const CharacterConfig* cfg = CharacterController::get(id);
      if (cfg != nullptr) {
        playerModel.characterId = cfg->id;
        playerModel.hasPhysics = true;
        playerModel.gravity = cfg->physics.gravity;
        playerModel.fallGravityMultiplier = cfg->jump.fallGravityMultiplier;
        if (cfg->math.subpixel > 1) {
          playerModel.hasMath = true;
          playerModel.subpixel = cfg->math.subpixel;
        }

        float gMul = 1.0F;
        if (playerModel.overlay.hasVel && playerModel.overlay.velY > 0.0F) {
          gMul *= std::max(0.0F, cfg->jump.fallGravityMultiplier);
        }
        if (playerModel.overlay.hasVel && playerModel.overlay.velY > 0.0F &&
            playerModel.overlay.hasActionState && playerModel.overlay.gliding) {
          gMul *= std::clamp(cfg->actions.glide.gravityMultiplier, 0.0F, 1.0F);
        }
        playerModel.effectiveGravityMultiplier = gMul;
      }

      if (playerModel.overlay.hasActionState || playerModel.overlay.hasGrounded ||
          playerModel.overlay.hasVel) {
        if (playerModel.overlay.hasActionState && playerModel.overlay.walling) {
          playerModel.movementState = "Wall";
        } else if (playerModel.overlay.hasActionState && playerModel.overlay.gliding) {
          playerModel.movementState = "Glide";
        } else if (playerModel.overlay.hasActionState && playerModel.overlay.flying) {
          playerModel.movementState = "Fly";
        } else if (playerModel.overlay.hasActionState &&
                   playerModel.overlay.dashRemainingFrames > 0) {
          playerModel.movementState = "Dash";
        } else if (playerModel.overlay.hasGrounded && playerModel.overlay.grounded) {
          playerModel.movementState = "Ground";
        } else if (playerModel.overlay.hasVel && playerModel.overlay.velY < 0.0F) {
          playerModel.movementState = "Jump";
        } else {
          playerModel.movementState = "Fall";
        }
      }
    }

    return playerModel;
  };

  auto makeControlsModel = [&]() {
    DebugUIControlsModel out{};

    input_.appendLegend(out.legend);

    if (world_.player == kInvalidEntity) {
      return out;
    }

    const CharacterConfig* cfg = CharacterController::get(world_.player);
    if (cfg == nullptr) {
      return out;
    }

    out.characterLabel = cfg->displayName.empty() ? cfg->id : cfg->displayName;
    if (out.characterLabel.empty()) {
      out.characterLabel = "player";
    }

    auto trigName = [](ActionTrigger t) -> const char* {
      return (t == ActionTrigger::Press) ? "press" : "hold";
    };
    auto inputName = [](ActionInput input) -> const char* {
      switch (input) {
        case ActionInput::Action1:
          return "action1";
        case ActionInput::Action2:
          return "action2";
        case ActionInput::Jump:
          return "jump";
      }
      return "action1";
    };

    if (cfg->actions.dash.enabled) {
      std::string airInfo = "air: no";
      if (cfg->actions.dash.allowAir) {
        if (cfg->actions.dash.airDashes < 0) {
          airInfo = "air: yes";
        } else {
          airInfo = "air: " + std::to_string(cfg->actions.dash.airDashes) + "/jump";
        }
      }
      out.actions.push_back("dash: " + std::string(trigName(cfg->actions.dash.trigger)) + " " +
                            inputName(cfg->actions.dash.input) + "  (" + airInfo + ")");
    }

    if (cfg->actions.spindash.enabled) {
      out.actions.push_back("spindash: hold " +
                            std::string(inputName(cfg->actions.spindash.input)) +
                            " (charge) + release  (ground)");
    }

    if (cfg->actions.spin.enabled) {
      out.actions.push_back("spin: " + std::string(trigName(cfg->actions.spin.trigger)) + " " +
                            inputName(cfg->actions.spin.input) + "  (ground)");
    }

    if (cfg->actions.fly.enabled) {
      out.actions.push_back("fly: " + std::string(trigName(cfg->actions.fly.trigger)) + " " +
                            inputName(cfg->actions.fly.input) + "  (air)");
    }

    if (cfg->actions.glide.enabled) {
      std::string mode = cfg->actions.glide.startOnPress
                             ? "start: press + hold"
                             : std::string(trigName(cfg->actions.glide.trigger));
      out.actions.push_back("glide: " + mode + " " + inputName(cfg->actions.glide.input) +
                            "  (air)");
    }

    auto hasPowerup = [&](std::string_view powerupId) -> bool {
      if (powerupId.empty()) {
        return true;
      }
      for (const auto& powerup : cfg->powerups) {
        if (powerup == powerupId) {
          return true;
        }
      }
      return false;
    };

    for (const auto& attack : cfg->actions.attacks) {
      if (!attack.enabled) {
        continue;
      }
      const bool locked = !hasPowerup(attack.powerupId);
      const char* style = attack.projectile.enabled ? "projectile" : "melee";
      std::string label = "attack";
      if (!attack.id.empty()) {
        label += " (" + attack.id + ")";
      }
      label += ": ";
      label += std::string(trigName(attack.trigger)) + " ";
      label += inputName(attack.input);
      label += "  (" + std::string(style);
      if (!attack.allowAir) {
        label += ", ground";
      } else {
        label += ", air";
      }
      if (locked) {
        label += ", locked";
      }
      label += ")";
      out.actions.push_back(label);
    }

    return out;
  };

  if (uiEnabled_ && panelsOpen_ && debugUi_.initialized()) {
    switch (internalResMode_) {
      case 0:
        chooseInternalResolution(lastGameAvailW_, lastGameAvailH_, internalW, internalH);
        break;
      case 1:
        internalW = 960;
        internalH = 540;
        break;
      case 2:
        internalW = 640;
        internalH = 360;
        break;
      case 3:
        internalW = 480;
        internalH = 270;
        break;
      case 4:
        internalW = 320;
        internalH = 180;
        break;
      default:
        chooseInternalResolution(lastGameAvailW_, lastGameAvailH_, internalW, internalH);
        break;
    }
    ensureGameTarget(internalW, internalH);

    DebugUIMenuModel menuModel{};
    menuModel.stagePath = stagePath_;
    menuModel.characterPath = characterPath_;
    menuModel.spawnPoint = spawnPoint_;
    menuModel.spawnX = spawnX_;
    menuModel.spawnY = spawnY_;
    menuModel.stagePaths = &menuStages_;
    menuModel.stageLabels = &menuStageLabels_;
    menuModel.characterPaths = &menuCharacters_;
    menuModel.characterLabels = &menuCharacterLabels_;
    menuModel.spawnPoints = &menuSpawns_;
    menuModel.simPaused = simPaused_;
    menuModel.timeScale = timeScale_;
    menuModel.autoReload = autoReload_;
    menuModel.internalResMode = internalResMode_;
    menuModel.integerScaleOnly = integerScaleOnly_;
    menuModel.gamepadDeadzone = gamepadDeadzone_;
    menuModel.visualRules.highlightWarm = Visual::gRules.highlightWarm;
    menuModel.visualRules.shadowCool = Visual::gRules.shadowCool;
    menuModel.visualRules.outlineScale = Visual::gRules.outlineScale;
    menuModel.visualRules.squashStretchMax = Visual::gRules.squashStretchMax;
    menuModel.visualRules.preserveMass = Visual::gRules.preserveMass;
    menuModel.visualRules.smearThreshold = Visual::gRules.smearThreshold;
    menuModel.visualRules.afterimageCount = Visual::gRules.afterimageCount;
    menuModel.visualRules.afterimageDecay = Visual::gRules.afterimageDecay;

    DebugUIStageInspectorModel stageModel{};
    stageModel.stagePath = stagePath_;
    stageModel.stageVersion = stage_.version();
    stageModel.stageId = stage_.id();
    stageModel.stageDisplay = stage_.displayName();
    const StageRenderStyle& render = stage_.renderStyle();
    stageModel.renderBgTopHex = rgbHex(render.bgTop);
    stageModel.renderBgBottomHex = rgbHex(render.bgBottom);
    stageModel.renderPlatformBaseHex = rgbHex(render.platformBase);
    stageModel.renderPlatformLightHex = rgbHex(render.platformLight);
    stageModel.renderPlatformDarkHex = rgbHex(render.platformDark);
    stageModel.renderPlatformHighlightHex = rgbHex(render.platformHighlight);
    stageModel.solidCount = stage_.solidCount();
    stageModel.slopeCount = stage_.slopeCount();
    stageModel.collisionGroundSnap = stage_.collisionGroundSnap();
    stageModel.collisionStepUp = stage_.collisionStepUp();
    stageModel.collisionSkin = stage_.collisionSkin();
    stageModel.spawnPoints = &menuSpawns_;
    stageModel.spawns = &menuSpawnInfos_;
    stageModel.cameraDeadzoneW = stage_.cameraDeadzoneW();
    stageModel.cameraDeadzoneH = stage_.cameraDeadzoneH();
    stageModel.cameraLookaheadX = stage_.cameraLookaheadX();
    stageModel.cameraLookaheadY = stage_.cameraLookaheadY();
    stageModel.hasCameraFollow =
        (stageModel.cameraDeadzoneW > 0.0F || stageModel.cameraDeadzoneH > 0.0F ||
         stageModel.cameraLookaheadX != 0.0F || stageModel.cameraLookaheadY != 0.0F);

    Rect worldBounds{};
    if (stage_.getWorldBounds(worldBounds)) {
      stageModel.hasWorldBounds = true;
      stageModel.worldX = worldBounds.x;
      stageModel.worldY = worldBounds.y;
      stageModel.worldW = worldBounds.w;
      stageModel.worldH = worldBounds.h;
    }

    Rect cameraBounds{};
    if (stage_.getCameraBounds(cameraBounds)) {
      stageModel.hasCameraBounds = true;
      stageModel.cameraX = cameraBounds.x;
      stageModel.cameraY = cameraBounds.y;
      stageModel.cameraW = cameraBounds.w;
      stageModel.cameraH = cameraBounds.h;
    }

    DebugUIPanelsResult panels =
        debugUi_.drawPanels(winW, winH, menuModel, stageModel, makePlayerInspectorModel(),
                            gameTarget_, gameTargetW_, gameTargetH_, makeControlsModel());
    lastGameAvailW_ = panels.gameAvailW;
    lastGameAvailH_ = panels.gameAvailH;
    applyMenuActions(panels.actions);
  }

  const bool renderGameToTexture = uiEnabled_ && panelsOpen_ && debugUi_.initialized() &&
                                   (gameTarget_ != nullptr) && gameTargetW_ > 0 && gameTargetH_ > 0;

  if (renderGameToTexture) {
    updateCamera(gameTargetW_, gameTargetH_);
  } else {
    updateCamera(winW, winH);
  }

  const SDL_Rect drawViewport =
      renderGameToTexture ? SDL_Rect{0, 0, gameTargetW_, gameTargetH_} : SDL_Rect{0, 0, winW, winH};

  if (renderGameToTexture) {
    (void)SDL_SetRenderTarget(renderer_, gameTarget_);
  }

  SDL_SetRenderViewport(renderer_, &drawViewport);

  stage_.renderBackground(*shapeRenderer_, drawViewport.w, drawViewport.h);

  stage_.render(*renderer_, *shapeRenderer_, world_, camX_, camY_);

  for (auto entity : world_.registry.view<EnemyTag>()) {
    auto* tr = world_.registry.try_get<Transform>(entity);
    auto* box = world_.registry.try_get<AABB>(entity);
    if (tr == nullptr || box == nullptr) {
      continue;
    }

    auto* cfg = world_.registry.try_get<EnemyConfig>(entity);
    auto* state = world_.registry.try_get<EnemyState>(entity);

    // Update enemy animation timer
    auto& animState = world_.registry.get_or_emplace<EnemyAnimState>(entity);
    animState.timer += lastTs_.dt;

    // Use sprite rendering if config has sprites
    if (cfg != nullptr && cfg->render.hasSprites()) {
      const int facing = (state != nullptr) ? state->facingX : 1;
      const float posX = tr->pos.x + box->w * 0.5F;
      const float posY = tr->pos.y + box->h;
      renderEnemySprite(entity, *cfg, facing, posX, posY);
    } else {
      // Fallback to colored rectangles
      EnemyConfig::Type type = EnemyConfig::Type::Walker;
      if (cfg != nullptr) {
        type = cfg->type;
      }
      const Visual::Color fill = enemyColorForType(type);
      const float x = tr->pos.x - camX_;
      const float y = tr->pos.y - camY_;
      const float w = box->w;
      const float h = box->h;
      shapeRenderer_->fillRect(x, y, w, h, fill, 3.0F);
    }
  }

  for (auto entity : world_.registry.view<ProjectileTag>()) {
    auto* tr = world_.registry.try_get<Transform>(entity);
    auto* box = world_.registry.try_get<AABB>(entity);
    if (tr == nullptr || box == nullptr) {
      continue;
    }

    bool fromEnemy = true;
    auto* ps = world_.registry.try_get<ProjectileState>(entity);
    if (ps != nullptr) {
      fromEnemy = ps->fromEnemy;
    }

    const float x = tr->pos.x - camX_;
    const float y = tr->pos.y - camY_;
    const float w = box->w;
    const float h = box->h;
    const float cx = x + w * 0.5F;
    const float cy = y + h * 0.5F;
    const float radius = std::max(w, h) * 0.6F;
    shapeRenderer_->fillCircle(cx, cy, radius * 1.6F, projectileGlowColor(fromEnemy));
    shapeRenderer_->fillCircle(cx, cy, radius, projectileColor(fromEnemy));
  }

  // Render collectibles
  for (auto entity : world_.registry.view<CollectibleTag>()) {
    auto* tr = world_.registry.try_get<Transform>(entity);
    auto* box = world_.registry.try_get<AABB>(entity);
    if (tr == nullptr || box == nullptr) {
      continue;
    }

    auto* cfgPtr = world_.registry.try_get<CollectibleConfig>(entity);
    auto* statePtr = world_.registry.try_get<CollectibleState>(entity);

    // Update animation timer
    if (statePtr != nullptr) {
      statePtr->animTimer += lastTs_.dt;
      statePtr->bobTimer += lastTs_.dt;
    }

    const float bobOffset =
        (statePtr != nullptr) ? std::sin(statePtr->bobTimer * 4.0F) * 3.0F : 0.0F;

    // Use sprite rendering if config has sprites
    if (cfgPtr != nullptr && cfgPtr->render.hasSprite()) {
      const CollectibleConfig& cfg = *cfgPtr;
      const std::string& spritePath = cfg.render.sprite;
      SDL_Texture* tex = sprites_.get(spritePath);
      if (tex) {
        const float scale = cfg.render.scale;
        const float frameW = static_cast<float>(cfg.render.frameW);
        const float frameH = static_cast<float>(cfg.render.frameH);
        const float dstW = frameW * scale;
        const float dstH = frameH * scale;

        // Calculate frame for animation
        int frame = 0;
        if (cfg.render.frames > 1 && statePtr != nullptr) {
          const float frameDuration = 1.0F / cfg.render.fps;
          const float totalDuration = frameDuration * static_cast<float>(cfg.render.frames);
          const float animTime = std::fmod(statePtr->animTimer, totalDuration);
          frame = static_cast<int>(animTime / frameDuration) % cfg.render.frames;
        }

        // Position collectible centered on its position
        const float posX = tr->pos.x + box->w * 0.5F;
        const float posY = tr->pos.y + box->h + bobOffset;
        const float dstX = posX - dstW * 0.5F + cfg.render.offsetX - camX_;
        const float dstY = posY - dstH + cfg.render.offsetY - camY_;

        const SDL_FRect srcRect = {
            static_cast<float>(frame) * frameW,
            0.0F,
            frameW,
            frameH,
        };
        const SDL_FRect dstRect = {dstX, dstY, dstW, dstH};
        SDL_RenderTexture(renderer_, tex, &srcRect, &dstRect);
      }
    } else {
      // Fallback to colored circle
      const float x = tr->pos.x - camX_;
      const float y = tr->pos.y - camY_ + bobOffset;
      const float w = box->w;
      const float h = box->h;
      const float cx = x + w * 0.5F;
      const float cy = y + h * 0.5F;
      const float radius = std::max(w, h) * 0.5F;

      // Gold color for collectibles
      constexpr Visual::Color glowColor{255, 215, 0, 80};
      constexpr Visual::Color coreColor{255, 215, 0, 255};
      shapeRenderer_->fillCircle(cx, cy, radius * 1.5F, glowColor);
      shapeRenderer_->fillCircle(cx, cy, radius, coreColor);
    }
  }

  for (auto entity : world_.registry.view<CharacterTag>()) {
    auto* trPtr = world_.registry.try_get<Transform>(entity);
    auto* boxPtr = world_.registry.try_get<AABB>(entity);
    if (trPtr == nullptr || boxPtr == nullptr) {
      continue;
    }

    const Transform& t = *trPtr;
    const AABB& box = *boxPtr;

    AnimStateId animId = AnimStateId::Idle;
    auto* animPtr = world_.registry.try_get<AnimState>(entity);
    if (animPtr != nullptr) {
      animId = animPtr->id;
    }

    int facing = 1;
    auto* asPtr = world_.registry.try_get<ActionState>(entity);
    if (asPtr != nullptr) {
      facing = asPtr->facingX;
      if (animId == AnimStateId::Spin && asPtr->spinning) {
        facing = asPtr->spinFacingX;
      }
    }

    float attackAnimT = 0.0F;
    if (asPtr != nullptr) {
      if (asPtr->attackAnimTotalFrames > 0) {
        const float total = static_cast<float>(asPtr->attackAnimTotalFrames);
        const float remaining = static_cast<float>(asPtr->attackAnimFrames);
        attackAnimT = std::clamp((total - remaining) / total, 0.0F, 1.0F);
      }
    }

    // Use shape-based character rendering
    const CharacterConfig* cfg = CharacterController::get(entity);

    // Select form based on character ID
    const Visual::CharacterForm* form = &puppetForm_;
    const Visual::FormStyle style = resolveFormStyle(*form, cfg);

    // Map AnimStateId to pose name
    const char* poseName = nullptr;
    switch (animId) {
      case AnimStateId::Idle:
        poseName = "idle";
        break;
      case AnimStateId::Run:
        poseName = "run1";  // Will animate between run poses
        break;
      case AnimStateId::Jump:
        poseName = "jump";
        break;
      case AnimStateId::Fall:
        poseName = "fall";
        break;
      case AnimStateId::Dash:
        poseName = "dash";
        break;
      case AnimStateId::Spin:
        poseName = "spin1";
        break;
      case AnimStateId::Fly:
        poseName = "fly";
        break;
      case AnimStateId::Glide:
        poseName = "glide";
        break;
      case AnimStateId::SpindashCharge:
        poseName = "spindash";
        break;
      case AnimStateId::AttackMelee:
        poseName = "attack_melee_strike";
        break;
      case AnimStateId::AttackShoot:
        poseName = "attack_shoot";
        break;
      default:
        poseName = "idle";
        break;
    }

    // Get pose (with optional animation for run)
    Visual::Pose pose = form->getPose(poseName);

    // Read velocity once (used for animation timing and effects).
    auto* velPtr = world_.registry.try_get<Velocity>(entity);
    float vx = 0.0F;
    float vy = 0.0F;
    if (velPtr != nullptr) {
      vx = velPtr->v.x;
      vy = velPtr->v.y;
    }
    const float speed = std::sqrt(vx * vx + vy * vy);
    const float simTime = static_cast<float>(lastTs_.frame) * lastTs_.dt;

    if (animId == AnimStateId::Idle) {
      auto itAnim = form->animations.find("idle");
      if (itAnim != form->animations.end()) {
        pose = form->evaluateAnimation("idle", simTime);
      }
    }

    // For running, animate between poses based on velocity
    if (animId == AnimStateId::Run) {
      float runTime = simTime;
      auto itAnim = form->animations.find("run");
      if (itAnim != form->animations.end() && itAnim->second.velocityBlend) {
        const Visual::Animation& a = itAnim->second;
        const float denom = std::max(1.0F, a.velocityMax - a.velocityMin);
        float t01 = (std::fabs(vx) - a.velocityMin) / denom;
        t01 = std::clamp(t01, 0.0F, 1.0F);
        // Scale time so the cycle speeds up as we approach velocityMax.
        runTime *= 0.6F + t01;  // 0.6x .. 1.6x
      }
      pose = form->evaluateAnimation("run", runTime);
    }

    if (animId == AnimStateId::Fly) {
      auto itAnim = form->animations.find("fly");
      if (itAnim != form->animations.end()) {
        pose = form->evaluateAnimation("fly", simTime);
      }
    }

    if (animId == AnimStateId::SpindashCharge) {
      float chargeTime = simTime;
      auto itAnim = form->animations.find("spindash_charge");
      if (itAnim != form->animations.end()) {
        float rate = 1.0F;
        if (cfg != nullptr && asPtr != nullptr) {
          const int denom = std::max(1, cfg->actions.spindash.chargeFrames);
          float t01 = static_cast<float>(asPtr->spindashChargeFrames) / static_cast<float>(denom);
          t01 = std::clamp(t01, 0.0F, 1.0F);
          rate = 0.6F + t01 * 1.6F;
        }
        chargeTime *= rate;
        pose = form->evaluateAnimation("spindash_charge", chargeTime);
      }
    }

    if (animId == AnimStateId::Spin) {
      float spinTime = simTime;
      auto itAnim = form->animations.find("spin");
      if (itAnim != form->animations.end()) {
        const Visual::Animation& a = itAnim->second;
        if (a.velocityBlend) {
          const float denom = std::max(1.0F, a.velocityMax - a.velocityMin);
          float t01 = (std::fabs(vx) - a.velocityMin) / denom;
          t01 = std::clamp(t01, 0.0F, 1.0F);
          spinTime *= 0.7F + t01 * 1.2F;
        }
        pose = form->evaluateAnimation("spin", spinTime);
      }
    }

    if (animId == AnimStateId::AttackMelee) {
      auto itAnim = form->animations.find("attack_melee");
      if (itAnim != form->animations.end()) {
        const Visual::Animation& a = itAnim->second;
        pose = form->evaluateAnimation("attack_melee", attackAnimT * a.duration);
      } else {
        pose = form->getPose("attack_melee_strike");
      }
    }

    if (animId == AnimStateId::AttackShoot) {
      auto itAnim = form->animations.find("attack_shoot");
      if (itAnim != form->animations.end()) {
        const Visual::Animation& a = itAnim->second;
        pose = form->evaluateAnimation("attack_shoot", attackAnimT * a.duration);
      } else {
        pose = form->getPose("attack_shoot");
      }
    }

    if (cfg != nullptr && !cfg->form.anchorDeltas.empty()) {
      Visual::Pose adjusted = pose;
      adjusted.anchors.clear();
      adjusted.anchors.reserve(form->skeleton.anchors.size());
      for (const auto& [anchorName, basePos] : form->skeleton.anchors) {
        (void)basePos;
        Visual::Vec2 pos = pose.getAnchor(anchorName, form->skeleton);
        auto itDelta = cfg->form.anchorDeltas.find(anchorName);
        if (itDelta != cfg->form.anchorDeltas.end()) {
          pos = pos + Visual::Vec2{itDelta->second.x, itDelta->second.y};
        }
        adjusted.anchors[anchorName] = pos;
      }
      pose = std::move(adjusted);
    }

    // Apply secondary motion (spring dynamics) per anchor.
    pose = Visual::applyDynamics(*form, pose, formRuntime_[entity], lastTs_.dt);
    // Apply IK after dynamics to lock hands/feet to targets.
    pose = Visual::applyIk(*form, pose, static_cast<float>(facing));

    bool grounded = false;
    auto* groundedPtr = world_.registry.try_get<Grounded>(entity);
    if (groundedPtr != nullptr) {
      grounded = groundedPtr->onGround;
    }

    // Add a little motion "squash & stretch" on top of authored poses.
    const float maxStretch = std::max(1.0F, Visual::gRules.squashStretchMax);
    const float minStretch = 1.0F / maxStretch;

    auto applySquash = [&](float sx, float sy) {
      pose.squashX = std::clamp(pose.squashX * sx, minStretch, maxStretch);
      pose.squashY = std::clamp(pose.squashY * sy, minStretch, maxStretch);
    };

    const float smearThreshold = std::max(1.0F, Visual::gRules.smearThreshold);
    float horizT = std::clamp(std::fabs(vx) / (smearThreshold * 1.5F), 0.0F, 1.0F);
    if (horizT > 0.0F) {
      const float stretch = 1.0F + horizT * 0.12F;
      if (Visual::gRules.preserveMass) {
        applySquash(stretch, 1.0F / stretch);
      } else {
        applySquash(stretch, 1.0F - horizT * 0.08F);
      }
    }

    if (!grounded) {
      float vT = std::clamp(std::fabs(vy) / 1600.0F, 0.0F, 1.0F);
      if (vT > 0.0F) {
        if (vy < 0.0F) {
          const float stretch = 1.0F + vT * 0.10F;
          if (Visual::gRules.preserveMass) {
            applySquash(1.0F / stretch, stretch);
          } else {
            applySquash(1.0F - vT * 0.06F, stretch);
          }
        } else {
          const float squash = 1.0F - vT * 0.07F;
          if (Visual::gRules.preserveMass) {
            applySquash(1.0F / squash, squash);
          } else {
            applySquash(1.0F + vT * 0.06F, squash);
          }
        }
      }
    }

    // Calculate character position (feet at bottom of box)
    Visual::Vec2 charPos{t.pos.x + box.w * 0.5F, t.pos.y + box.h};

    const bool isPlayer = (entity == world_.player);
    if (isPlayer) {
      const int trailCount = std::clamp(Visual::gRules.afterimageCount, 0, kMaxAfterimages);
      const float decay = std::max(0.01F, Visual::gRules.afterimageDecay);
      const float threshold = std::max(0.0F, Visual::gRules.smearThreshold);
      const float interval = (trailCount > 0) ? (decay / static_cast<float>(trailCount)) : 0.0F;

      // Record afterimage frames while moving fast.
      afterimageTimer_ += lastTs_.dt;
      if (trailCount > 0 && speed > threshold && afterimageTimer_ >= interval) {
        afterimageTimer_ = 0.0F;

        AfterimageFrame& frame = afterimages_[afterimageIndex_];
        frame.pos = charPos;
        frame.facing = static_cast<float>(facing);
        frame.pose = pose;
        frame.age = 0.0F;

        const int ring = std::max(1, trailCount);
        afterimageIndex_ = (afterimageIndex_ + 1) % ring;
      }

      // Age the trail and render it (older first).
      for (int i = 0; i < trailCount; ++i) {
        afterimages_[i].age += lastTs_.dt;
      }

      for (int i = 0; i < trailCount; ++i) {
        const AfterimageFrame& frame = afterimages_[i];
        if (frame.age <= 0.0F || frame.age > decay) {
          continue;
        }

        float alpha = 1.0F - (frame.age / decay);
        alpha = std::clamp(alpha, 0.0F, 1.0F);
        uint8_t a = static_cast<uint8_t>(alpha * 160);  // softer than full alpha

        Visual::Color tint = form->palette.base;
        tint.a = a;

        formRenderer_->renderAfterimage(*form, frame.pose, frame.pos, frame.facing, tint, camX_,
                                        camY_, &style);
      }
    }

    // Render the main character - use sprites if configured, otherwise form
    const bool useSpriteRender = cfg != nullptr && cfg->render.useSpriteRendering();
    if (useSpriteRender) {
      renderCharacterSprite(entity, *cfg, animId, facing, charPos.x, charPos.y, simTime);
    } else {
      formRenderer_->renderWithOutline(*form, pose, charPos, static_cast<float>(facing), camX_,
                                       camY_, &style);
    }
  }

  auto drawOutlineRect = [&](float x, float y, float w, float h, float thickness,
                             Visual::Color color) {
    if (w <= 0.0F || h <= 0.0F || thickness <= 0.0F) {
      return;
    }
    const float t = std::min(thickness, std::min(w, h) * 0.5F);
    shapeRenderer_->fillRect(x, y, w, t, color);
    shapeRenderer_->fillRect(x, y + h - t, w, t, color);
    shapeRenderer_->fillRect(x, y, t, h, color);
    shapeRenderer_->fillRect(x + w - t, y, t, h, color);
  };

  for (auto entity : world_.registry.view<AttackHitboxTag>()) {
    auto* tr = world_.registry.try_get<Transform>(entity);
    auto* box = world_.registry.try_get<AABB>(entity);
    auto* statePtr = world_.registry.try_get<AttackHitboxState>(entity);
    if (tr == nullptr || box == nullptr || statePtr == nullptr) {
      continue;
    }

    const AttackHitboxState& state = *statePtr;
    const bool startup = state.startupFrames > 0;
    const bool active = (!startup && state.activeFrames > 0);
    if (!startup && !active) {
      continue;
    }

    const float x = tr->pos.x - camX_;
    const float y = tr->pos.y - camY_;
    const float w = box->w;
    const float h = box->h;
    const Visual::Color fill = hitboxFillColor(state.fromPlayer, active);
    const Visual::Color outline = hitboxOutlineColor(state.fromPlayer, active);
    shapeRenderer_->fillRect(x, y, w, h, fill, 2.0F);
    drawOutlineRect(x, y, w, h, 2.0F, outline);
  }

  renderCharacterSwapZap();

  if (debugCollision_) {
    stage_.renderDebugCollision(*renderer_, world_, camX_, camY_);
    renderDebugCamera(drawViewport.w, drawViewport.h);
  }

  if (!panelsOpen_) {
    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
    if (debugUi_.initialized()) {
      SDL_RenderDebugText(renderer_, 8.0F, 8.0F, "Ctrl-H: show UI");
    }
  }
  if (renderGameToTexture) {
    (void)SDL_SetRenderTarget(renderer_, nullptr);
  }
  SDL_SetRenderViewport(renderer_, nullptr);

  if (debugOverlay_) {
    if (debugUi_.initialized()) {
      debugUi_.drawOverlay(makeOverlayModel());
    } else {
      renderDebugOverlay();
    }
  }

  if (debugUi_.initialized()) {
    uiCaptureKeyboard_ = debugUi_.wantCaptureKeyboard();
    uiCaptureMouse_ = debugUi_.wantCaptureMouse();
    debugUi_.endFrame(renderer_);
  } else {
    uiCaptureKeyboard_ = false;
    uiCaptureMouse_ = false;
  }

  updateWindowResizeCursors(winW, winH);
  SDL_RenderPresent(renderer_);
}
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers)

void App::updateWindowResizeCursors(int winW, int winH) {
  if (window_ == nullptr) {
    return;
  }
  if (winW <= 0 || winH <= 0) {
    return;
  }
  if ((SDL_GetWindowFlags(window_) & SDL_WINDOW_RESIZABLE) == 0) {
    return;
  }
  if ((cursorNwResize_ == nullptr) || (cursorNeResize_ == nullptr) ||
      (cursorSeResize_ == nullptr) || (cursorSwResize_ == nullptr)) {
    return;
  }

  if (SDL_GetMouseFocus() != window_) {
    return;
  }

  float mx = 0.0F;
  float my = 0.0F;
  (void)SDL_GetMouseState(&mx, &my);

  static constexpr int kCornerPx = 12;
  const int x = static_cast<int>(mx);
  const int y = static_cast<int>(my);

  const bool left = (x >= 0 && x < kCornerPx);
  const bool right = (x >= winW - kCornerPx && x < winW);
  const bool top = (y >= 0 && y < kCornerPx);
  const bool bottom = (y >= winH - kCornerPx && y < winH);

  SDL_Cursor* desired = nullptr;
  if (top && left) {
    desired = cursorNwResize_;
  } else if (top && right) {
    desired = cursorNeResize_;
  } else if (bottom && right) {
    desired = cursorSeResize_;
  } else if (bottom && left) {
    desired = cursorSwResize_;
  }

  if (desired != nullptr) {
    SDL_SetCursor(desired);
  } else if (!debugUi_.initialized() && (cursorDefault_ != nullptr)) {
    SDL_SetCursor(cursorDefault_);
  }
}

static void drawDebugCross(SDL_Renderer* r, float x, float y, float radius) {
  (void)SDL_RenderLine(r, x - radius, y, x + radius, y);
  (void)SDL_RenderLine(r, x, y - radius, x, y + radius);
}

static void drawZapBurst(SDL_Renderer* r, float x, float y, float radius, uint8_t alpha) {
  static constexpr float kPi = 3.14159265F;
  static constexpr int kRays = 8;
  const float inner = radius * 0.35F;
  SDL_SetRenderDrawColor(r, 255, 240, 140, alpha);
  for (int i = 0; i < kRays; ++i) {
    const float a = (kPi * 2.0F) * (static_cast<float>(i) / static_cast<float>(kRays));
    const float dx = std::cos(a);
    const float dy = std::sin(a);
    (void)SDL_RenderLine(r, x + dx * inner, y + dy * inner, x + dx * radius, y + dy * radius);
  }
}

void App::renderCharacterSwapZap() {
  if (renderer_ == nullptr || characterSwapZapTimer_ <= 0.0F) {
    return;
  }

  const float t =
      std::clamp(characterSwapZapTimer_ / std::max(0.001F, characterSwapZapDuration_), 0.0F, 1.0F);
  const float radius = 14.0F + (1.0F - t) * 18.0F;
  const uint8_t alpha = static_cast<uint8_t>(std::clamp(t * 255.0F, 0.0F, 255.0F));

  const float screenX = characterSwapZapPos_.x - camX_;
  const float screenY = characterSwapZapPos_.y - camY_;

  SDL_BlendMode prevBlend = SDL_BLENDMODE_NONE;
  (void)SDL_GetRenderDrawBlendMode(renderer_, &prevBlend);
  SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
  drawZapBurst(renderer_, screenX, screenY, radius, alpha);
  SDL_SetRenderDrawBlendMode(renderer_, prevBlend);
}

void App::renderDebugCamera(int viewW, int viewH) {
  if (renderer_ == nullptr) {
    return;
  }
  if (viewW <= 0 || viewH <= 0) {
    return;
  }

  const float viewHalfW = static_cast<float>(viewW) * 0.5F;
  const float viewHalfH = static_cast<float>(viewH) * 0.5F;

  const float dzW = stage_.cameraDeadzoneW();
  const float dzH = stage_.cameraDeadzoneH();
  const float halfDzW = std::min(std::max(0.0F, dzW) * 0.5F, viewHalfW);
  const float halfDzH = std::min(std::max(0.0F, dzH) * 0.5F, viewHalfH);

  if (halfDzW > 0.0F || halfDzH > 0.0F) {
    SDL_SetRenderDrawColor(renderer_, 0, 255, 0, 255);
    SDL_FRect fr{viewHalfW - halfDzW, viewHalfH - halfDzH, halfDzW * 2.0F, halfDzH * 2.0F};
    SDL_RenderRect(renderer_, &fr);
    SDL_RenderDebugText(renderer_, 8.0F, 52.0F, "green: camera deadzone");
  }

  if (world_.player == kInvalidEntity) {
    return;
  }

  auto* trPtr = world_.registry.try_get<Transform>(world_.player);
  if (trPtr == nullptr) {
    return;
  }

  float halfW = 0.0F;
  float halfH = 0.0F;
  auto* boxPtr = world_.registry.try_get<AABB>(world_.player);
  if (boxPtr != nullptr) {
    halfW = boxPtr->w * 0.5F;
    halfH = boxPtr->h * 0.5F;
  }

  const float centerX = trPtr->pos.x + halfW;
  const float centerY = trPtr->pos.y + halfH;

  int facing = 1;
  auto* asPtr = world_.registry.try_get<ActionState>(world_.player);
  if (asPtr != nullptr) {
    facing = (asPtr->facingX < 0) ? -1 : 1;
  } else {
    auto* velPtr = world_.registry.try_get<Velocity>(world_.player);
    if (velPtr != nullptr) {
      if (velPtr->v.x > 0.0F) {
        facing = 1;
      } else if (velPtr->v.x < 0.0F) {
        facing = -1;
      }
    }
  }

  const float lookaheadX = stage_.cameraLookaheadX() * static_cast<float>(facing);
  const float lookaheadY = stage_.cameraLookaheadY();
  const float targetX = centerX + lookaheadX;
  const float targetY = centerY + lookaheadY;

  const float px = centerX - camX_;
  const float py = centerY - camY_;
  const float tx = targetX - camX_;
  const float ty = targetY - camY_;

  SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
  drawDebugCross(renderer_, viewHalfW, viewHalfH, 6.0F);
  drawDebugCross(renderer_, px, py, 4.0F);

  if (lookaheadX != 0.0F || lookaheadY != 0.0F) {
    SDL_SetRenderDrawColor(renderer_, 0, 255, 255, 255);
    (void)SDL_RenderLine(renderer_, px, py, tx, ty);
    drawDebugCross(renderer_, tx, ty, 4.0F);
    SDL_RenderDebugText(renderer_, 8.0F, 64.0F, "cyan: lookahead target");
  }
}

void App::renderDebugOverlay() {
  static constexpr float kX = 8.0F;
  static constexpr float kY = 8.0F;
  static constexpr float kLineH = static_cast<float>(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) + 2.0F;

  float y = kY;

  SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
  SDL_RenderDebugTextFormat(renderer_, kX, y, "frame: %llu  dt: %.5F",
                            static_cast<unsigned long long>(lastTs_.frame), lastTs_.dt);
  y += kLineH;

  SDL_RenderDebugTextFormat(renderer_, kX, y, "debug: overlay=%d collision=%d  (Ctrl-1, Ctrl-2)",
                            debugOverlay_ ? 1 : 0, debugCollision_ ? 1 : 0);
  y += kLineH;

  SDL_RenderDebugText(renderer_, kX, y, "quit: Ctrl-C");
  y += kLineH;

  SDL_RenderDebugTextFormat(renderer_, kX, y, "camera: (%.1F, %.1F)", camX_, camY_);
  y += kLineH;

  if (world_.player == kInvalidEntity) {
    return;
  }

  const EntityId id = world_.player;
  auto* namePtr = world_.registry.try_get<DebugName>(id);
  auto* trPtr = world_.registry.try_get<Transform>(id);
  auto* velPtr = world_.registry.try_get<Velocity>(id);
  auto* groundedPtr = world_.registry.try_get<Grounded>(id);
  auto* coyotePtr = world_.registry.try_get<CoyoteFrames>(id);
  auto* jbufPtr = world_.registry.try_get<JumpBufferFrames>(id);
  auto* acdPtr = world_.registry.try_get<ActionCooldownFrames>(id);
  auto* atkPtr = world_.registry.try_get<AttackCooldownFrames>(id);
  auto* asPtr = world_.registry.try_get<ActionState>(id);

  if (namePtr != nullptr) {
    SDL_RenderDebugTextFormat(renderer_, kX, y, "player: %u  name: %s", static_cast<uint32_t>(id),
                              namePtr->name.c_str());
    y += kLineH;
  } else {
    SDL_RenderDebugTextFormat(renderer_, kX, y, "player: %u", static_cast<uint32_t>(id));
    y += kLineH;
  }

  if (trPtr != nullptr) {
    SDL_RenderDebugTextFormat(renderer_, kX, y, "pos: (%.1F, %.1F)", trPtr->pos.x, trPtr->pos.y);
    y += kLineH;
  }

  if (velPtr != nullptr) {
    SDL_RenderDebugTextFormat(renderer_, kX, y, "vel: (%.1F, %.1F)", velPtr->v.x, velPtr->v.y);
    y += kLineH;
  }

  if (groundedPtr != nullptr) {
    SDL_RenderDebugTextFormat(renderer_, kX, y, "grounded: %d", groundedPtr->onGround ? 1 : 0);
    y += kLineH;
  }

  if (coyotePtr != nullptr && jbufPtr != nullptr && acdPtr != nullptr && atkPtr != nullptr) {
    SDL_RenderDebugTextFormat(renderer_, kX, y,
                              "coyote_ms=%d  jumpbuf_ms=%d  action_cd_ms=%d  attack_cd_ms=%d",
                              coyotePtr->value, jbufPtr->value, acdPtr->value, atkPtr->value);
    y += kLineH;
  }

  if (asPtr != nullptr) {
    SDL_RenderDebugTextFormat(
        renderer_, kX, y, "action: dash_ms=%d air_used=%d  spindash=%d(%dms)  spin=%d  glide=%d",
        asPtr->dashRemainingFrames, asPtr->airDashesUsed, asPtr->spindashCharging ? 1 : 0,
        asPtr->spindashChargeFrames, asPtr->spinning ? 1 : 0, asPtr->gliding ? 1 : 0);
  }
}

static std::string stageMenuLabelForPath(const std::string& path) {
  Stage stage;
  if (stage.loadFromToml(path.c_str())) {
    if (!stage.displayName().empty() && !stage.id().empty() && stage.displayName() != stage.id()) {
      return stage.displayName() + " (" + stage.id() + ")";
    }
    if (!stage.displayName().empty()) {
      return stage.displayName();
    }
    if (!stage.id().empty()) {
      return stage.id();
    }
  }
  return Paths::fileStem(path);
}

static std::string characterMenuLabelForPath(const std::string& path) {
  CharacterConfig cfg;
  if (cfg.loadFromToml(path.c_str())) {
    std::string display = cfg.displayName.empty() ? cfg.id : cfg.displayName;
    if (!display.empty() && !cfg.id.empty() && display != cfg.id) {
      return display + " (" + cfg.id + ")";
    }
    if (!display.empty()) {
      return display;
    }
  }
  return Paths::fileStem(path);
}

void App::refreshMenuLists() {
  menuStages_.clear();
  menuStageLabels_.clear();
  menuCharacters_.clear();
  menuCharacterLabels_.clear();

  menuStages_ = Paths::listTomlFiles(Paths::resolveAssetPath("assets/stages"));
  menuCharacters_ = Paths::listTomlFiles(Paths::resolveAssetPath("assets/characters"));

  menuStageLabels_.reserve(menuStages_.size());
  for (const auto& p : menuStages_) {
    menuStageLabels_.push_back(stageMenuLabelForPath(p));
  }

  menuCharacterLabels_.reserve(menuCharacters_.size());
  for (const auto& p : menuCharacters_) {
    menuCharacterLabels_.push_back(characterMenuLabelForPath(p));
  }

  refreshSpawnLists();
}

void App::refreshSpawnLists() {
  menuSpawns_.clear();
  menuSpawnInfos_.clear();

  menuSpawns_ = stage_.spawnNames();
  menuSpawnInfos_.reserve(menuSpawns_.size());
  for (const auto& name : menuSpawns_) {
    float x = 0.0F;
    float y = 0.0F;
    int facing = 1;
    if (!stage_.getSpawn(name.c_str(), x, y, facing)) {
      continue;
    }
    menuSpawnInfos_.push_back(DebugUIStageInspectorModel::SpawnInfo{name, x, y, facing});
  }
}

void App::respawnEnemies() {
  std::vector<EntityId> toDestroy;
  auto view = world_.registry.view<EnemyTag>();
  toDestroy.reserve(view.size());
  for (auto entity : view) {
    toDestroy.push_back(entity);
  }

  for (EntityId id : toDestroy) {
    world_.destroy(id);
  }

  stage_.spawnEnemies(world_);
}

void App::respawnCollectibles() {
  std::vector<EntityId> toDestroy;
  auto view = world_.registry.view<CollectibleTag>();
  toDestroy.reserve(view.size());
  for (auto entity : view) {
    toDestroy.push_back(entity);
  }

  for (EntityId id : toDestroy) {
    world_.destroy(id);
  }

  stage_.spawnCollectibles(world_);
}

void App::respawnPlayer() {
  ++respawnCount_;

  if (world_.player != kInvalidEntity) {
    CharacterController::detach(world_.player);
    world_.destroy(world_.player);
    world_.player = kInvalidEntity;
  }

  float sx = spawnX_;
  float sy = spawnY_;
  int facing = 1;
  if (!spawnPoint_.empty()) {
    if (!stage_.getSpawn(spawnPoint_.c_str(), sx, sy, facing)) {
      spawnPoint_.clear();
    }
  }

  Stage::spawnDemo(world_, characterPath_.c_str(), sx, sy, facing);
  respawnEnemies();
  respawnCollectibles();

  // Clear cosmetic trail state so we don't leave ghosts on respawn.
  afterimageIndex_ = 0;
  afterimageTimer_ = 0.0F;
  for (auto& frame : afterimages_) {
    frame.age = 999.0F;
  }

  // Snap camera to new player position on next frame
  cameraSnapPending_ = true;
  cameraTrackValid_ = false;
  cameraLookOffsetY_ = 0.0F;
  lookUpHeldMs_ = 0;
  lookDownHeldMs_ = 0;
}

void App::teleportPlayerToSpawn() {
  if (world_.player == kInvalidEntity) {
    return;
  }

  float sx = spawnX_;
  float sy = spawnY_;
  int facing = 1;
  if (!spawnPoint_.empty()) {
    if (!stage_.getSpawn(spawnPoint_.c_str(), sx, sy, facing)) {
      return;
    }
  }

  const EntityId id = world_.player;
  auto* trPtr = world_.registry.try_get<Transform>(id);
  if (trPtr != nullptr) {
    trPtr->pos = Vec2{sx, sy};
  } else {
    world_.registry.emplace<Transform>(id, Vec2{sx, sy});
  }

  auto* asPtr = world_.registry.try_get<ActionState>(id);
  if (asPtr != nullptr) {
    asPtr->facingX = (facing < 0) ? -1 : 1;
  }
  CharacterController::setFacing(id, facing);

  auto* groundedPtr = world_.registry.try_get<Grounded>(id);
  if (groundedPtr != nullptr) {
    groundedPtr->onGround = false;
  }

  afterimageIndex_ = 0;
  afterimageTimer_ = 0.0F;
  for (auto& frame : afterimages_) {
    frame.age = 999.0F;
  }

  resetPlayerState();
}

bool App::reloadStageFromDisk() {
  if (!stage_.loadFromToml(stagePath_.c_str())) {
    std::cerr << "Failed to load stage TOML: " << stagePath_ << '\n';
    return false;
  }

  refreshSpawnLists();

  if (!spawnPoint_.empty()) {
    float sx = 0.0F;
    float sy = 0.0F;
    int facing = 1;
    if (!stage_.getSpawn(spawnPoint_.c_str(), sx, sy, facing)) {
      spawnPoint_ = "default";
    }
  }

  return true;
}

bool App::reloadCharacterFromDiskInPlace(bool resetRuntime) {
  if (world_.player == kInvalidEntity) {
    return false;
  }

  CharacterConfig cfg;
  if (!cfg.loadFromToml(characterPath_.c_str())) {
    std::cerr << "Failed to load character TOML: " << characterPath_ << '\n';
    return false;
  }

  const std::string displayName = cfg.displayName;
  const float hitboxW = cfg.collision.w;
  const float hitboxH = cfg.collision.h;
  const EntityId id = world_.player;

  if (!CharacterController::replaceConfig(id, std::move(cfg), resetRuntime)) {
    return false;
  }

  auto* boxPtr = world_.registry.try_get<AABB>(id);
  if (boxPtr != nullptr) {
    boxPtr->w = hitboxW;
    boxPtr->h = hitboxH;
  }

  auto* namePtr = world_.registry.try_get<DebugName>(id);
  if (namePtr != nullptr) {
    namePtr->name = displayName;
  } else {
    world_.registry.emplace<DebugName>(id, displayName);
  }

  return true;
}

bool App::swapCharacterInPlace(const std::string& nextPath, bool resetRuntime) {
  if (nextPath.empty()) {
    return false;
  }

  if (nextPath == characterPath_) {
    startCharacterSwapZap();
    return true;
  }

  if (world_.player == kInvalidEntity) {
    characterPath_ = nextPath;
    respawnPlayer();
    startCharacterSwapZap();
    return true;
  }

  const std::string prevPath = characterPath_;
  characterPath_ = nextPath;
  if (!reloadCharacterFromDiskInPlace(resetRuntime)) {
    characterPath_ = prevPath;
    return false;
  }

  startCharacterSwapZap();
  return true;
}

void App::startCharacterSwapZap() {
  if (world_.player == kInvalidEntity) {
    return;
  }

  const EntityId id = world_.player;
  auto* trPtr = world_.registry.try_get<Transform>(id);
  auto* boxPtr = world_.registry.try_get<AABB>(id);
  if (trPtr == nullptr || boxPtr == nullptr) {
    return;
  }

  characterSwapZapPos_ = Visual::Vec2{trPtr->pos.x + boxPtr->w * 0.5F, trPtr->pos.y + boxPtr->h};
  characterSwapZapTimer_ = characterSwapZapDuration_;
}

void App::resetPlayerState() {
  if (world_.player == kInvalidEntity) {
    return;
  }
  const EntityId id = world_.player;

  cameraLookOffsetY_ = 0.0F;
  lookUpHeldMs_ = 0;
  lookDownHeldMs_ = 0;

  auto* velPtr = world_.registry.try_get<Velocity>(id);
  if (velPtr != nullptr) {
    velPtr->v = Vec2{0.0F, 0.0F};
  }

  auto* coyotePtr = world_.registry.try_get<CoyoteFrames>(id);
  if (coyotePtr != nullptr) {
    coyotePtr->value = 0;
  }

  auto* jbufPtr = world_.registry.try_get<JumpBufferFrames>(id);
  if (jbufPtr != nullptr) {
    jbufPtr->value = 0;
  }

  auto* acdPtr = world_.registry.try_get<ActionCooldownFrames>(id);
  if (acdPtr != nullptr) {
    acdPtr->value = 0;
  }

  auto* atkPtr = world_.registry.try_get<AttackCooldownFrames>(id);
  if (atkPtr != nullptr) {
    atkPtr->value = 0;
  }

  auto* dropPtr = world_.registry.try_get<DropThroughFrames>(id);
  if (dropPtr != nullptr) {
    dropPtr->value = 0;
  }

  auto* inputPtr = world_.registry.try_get<InputState>(id);
  if (inputPtr != nullptr) {
    *inputPtr = InputState{};
  }

  CharacterController::resetRuntime(id);
}

// NOLINTNEXTLINE
void App::run(int maxFrames) {
  static constexpr double kFixedDt = 1.0 / 120.0;

  simFrame_ = 0;
  cameraBackscrolled_ = false;
  cameraTrackValid_ = false;

  auto suppressGameplayInput = [&]() -> bool {
    if (!debugUi_.initialized()) {
      return false;
    }
    if (uiCaptureKeyboard_) {
      return true;
    }
    if (!uiCaptureMouse_) {
      return false;
    }

    float mx = 0.0F;
    float my = 0.0F;
    const SDL_MouseButtonFlags buttons = SDL_GetMouseState(&mx, &my);
    (void)mx;
    (void)my;
    const SDL_MouseButtonFlags any =
        static_cast<SDL_MouseButtonFlags>(SDL_BUTTON_LMASK | SDL_BUTTON_MMASK | SDL_BUTTON_RMASK);
    return (buttons & any) != 0;
  };

  if (maxFrames >= 0) {
    if (!renderEnabled_) {
      while (running_ && simFrame_ < static_cast<uint64_t>(maxFrames)) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
          handleEvent(e);
          input_.handleEvent(e);
        }

        const AppCommands cmds = input_.consumeCommands();
        handleCommands(cmds);
        if (!running_) {
          break;
        }

        maybeAutoReload();

        InputState gameInput = input_.consume();
        if (inputScriptEnabled_) {
          gameInput = inputScript_.sample(simFrame_);
        }
        if (world_.player != kInvalidEntity) {
          world_.registry.get_or_emplace<InputState>(world_.player) = gameInput;
        }
        tick(TimeStep{static_cast<float>(kFixedDt), simFrame_++});
      }

      return;
    }

    while (running_ && simFrame_ < static_cast<uint64_t>(maxFrames)) {
      SDL_Event e;
      while (SDL_PollEvent(&e)) {
        handleEvent(e);
        debugUi_.processEvent(e);
        input_.handleEvent(e);
      }

      const AppCommands cmds = input_.consumeCommands();
      handleCommands(cmds);
      if (!running_) {
        break;
      }

      maybeAutoReload();

      if (simPaused_ && pendingSimSteps_ <= 0) {
        (void)input_.consume();  // clear edges while paused
        if (renderEnabled_) {
          render();
        }
        continue;
      }

      int stepsToRun = 1;
      if (simPaused_ && pendingSimSteps_ > 0) {
        static constexpr int kMaxStepBurst = 240;
        stepsToRun = std::max(1, std::min(pendingSimSteps_, kMaxStepBurst));
      }

      for (int i = 0; i < stepsToRun && simFrame_ < static_cast<uint64_t>(maxFrames); ++i) {
        InputState gameInput = input_.consume();
        if (inputScriptEnabled_) {
          gameInput = inputScript_.sample(simFrame_);
        }
        if (suppressGameplayInput()) {
          gameInput = InputState{};
        }
        if (world_.player != kInvalidEntity) {
          world_.registry.get_or_emplace<InputState>(world_.player) = gameInput;
        }
        tick(TimeStep{static_cast<float>(kFixedDt), simFrame_++});
        if (simPaused_ && pendingSimSteps_ > 0) {
          --pendingSimSteps_;
        }
      }
      if (renderEnabled_) {
        render();
      }
    }

    return;
  }

  uint64_t last = SDL_GetTicksNS();
  double accumulator = 0.0;
  static constexpr double kMaxFrameTime = 0.25;  // seconds
  static constexpr int kMaxStepsPerFrame = 8;

  while (running_) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      handleEvent(e);
      debugUi_.processEvent(e);
      input_.handleEvent(e);
    }

    const AppCommands cmds = input_.consumeCommands();
    handleCommands(cmds);
    if (!running_) {
      break;
    }

    maybeAutoReload();

    if (simPaused_) {
      const int maxStepBurst = 240;
      int stepsToRun = std::min(pendingSimSteps_, maxStepBurst);
      if (stepsToRun <= 0) {
        (void)input_.consume();  // clear edges while paused
      } else {
        for (int i = 0; i < stepsToRun; ++i) {
          InputState gameInput = input_.consume();
          if (inputScriptEnabled_) {
            gameInput = inputScript_.sample(simFrame_);
          }
          if (suppressGameplayInput()) {
            gameInput = InputState{};
          }
          if (world_.player != kInvalidEntity) {
            world_.registry.get_or_emplace<InputState>(world_.player) = gameInput;
          }
          tick(TimeStep{static_cast<float>(kFixedDt), simFrame_++});
          --pendingSimSteps_;
        }
      }

      last = SDL_GetTicksNS();
      accumulator = 0.0;
      if (renderEnabled_) {
        render();
      }
      continue;
    }

    uint64_t now = SDL_GetTicksNS();
    double frameTime = static_cast<double>(now - last) / 1'000'000'000.0;
    last = now;
    frameTime = std::min(frameTime, kMaxFrameTime);
    accumulator += frameTime * static_cast<double>(timeScale_);

    int steps = 0;
    while (accumulator >= kFixedDt && steps < kMaxStepsPerFrame) {
      InputState gameInput = input_.consume();
      if (inputScriptEnabled_) {
        gameInput = inputScript_.sample(simFrame_);
      }
      if (suppressGameplayInput()) {
        gameInput = InputState{};
      }
      if (world_.player != kInvalidEntity) {
        world_.registry.get_or_emplace<InputState>(world_.player) = gameInput;
      }

      tick(TimeStep{static_cast<float>(kFixedDt), simFrame_++});

      accumulator -= kFixedDt;
      ++steps;
    }

    if (steps == kMaxStepsPerFrame) {
      accumulator = 0.0;
    }

    if (renderEnabled_) {
      render();
    }
  }
}

static bool readLastWriteTime(const std::string& path, std::filesystem::file_time_type& outTime) {
  if (path.empty()) {
    return false;
  }
  std::error_code ec;
  outTime = std::filesystem::last_write_time(path, ec);
  return !ec;
}

void App::syncWatchedFiles() {
  stageMtimeValid_ = readLastWriteTime(stagePath_, stageMtime_);
  characterMtimeValid_ = readLastWriteTime(characterPath_, characterMtime_);
  formMtimeValid_ = readLastWriteTime(formPath_, formMtime_);
  lastAutoReloadCheckNs_ = SDL_GetTicksNS();
}

void App::maybeAutoReload() {
  if (!autoReload_) {
    return;
  }

  static constexpr uint64_t kPollNs = 250'000'000ULL;  // 250ms
  const uint64_t now = SDL_GetTicksNS();
  if (lastAutoReloadCheckNs_ != 0 && now - lastAutoReloadCheckNs_ < kPollNs) {
    return;
  }
  lastAutoReloadCheckNs_ = now;

  std::filesystem::file_time_type stageNow{};
  if (stageMtimeValid_ && readLastWriteTime(stagePath_, stageNow) && stageNow != stageMtime_) {
    stageMtime_ = stageNow;
    if (reloadStageFromDisk()) {
      respawnPlayer();
    }
  }

  std::filesystem::file_time_type characterNow{};
  if (characterMtimeValid_ && readLastWriteTime(characterPath_, characterNow) &&
      characterNow != characterMtime_) {
    characterMtime_ = characterNow;
    (void)reloadCharacterFromDiskInPlace(false);
  }

  std::filesystem::file_time_type formNow{};
  if (formMtimeValid_ && readLastWriteTime(formPath_, formNow) && formNow != formMtime_) {
    formMtime_ = formNow;
    rebuildCharacterForms();
  }
}

void App::renderCharacterSprite(EntityId id,
                                const CharacterConfig& cfg,
                                AnimStateId animId,
                                int facing,
                                float posX,
                                float posY,
                                float simTime) {
  (void)id;  // May be used for entity-specific effects later

  // Map AnimStateId to render config animation name
  const char* animName = nullptr;
  switch (animId) {
    case AnimStateId::Idle:
      animName = "idle";
      break;
    case AnimStateId::Run:
      animName = "run";
      break;
    case AnimStateId::Jump:
    case AnimStateId::Fall:
      animName = "jump";
      break;
    case AnimStateId::Dash:
    case AnimStateId::Spin:
    case AnimStateId::Fly:
    case AnimStateId::SpindashCharge:
      animName = "special";
      break;
    case AnimStateId::Glide:
      animName = "jump";  // Fallback for glide
      break;
    case AnimStateId::AttackMelee:
    case AnimStateId::AttackShoot:
      animName = "attack";
      break;
  }

  if (animName == nullptr) {
    return;
  }

  // Get the animation clip
  auto itClip = cfg.render.anims.find(animName);
  if (itClip == cfg.render.anims.end()) {
    return;
  }
  const CharacterConfig::RenderClip& clip = itClip->second;

  // Get the sprite sheet for this facing direction
  const std::string& sheetPath = cfg.render.getSheetForFacing(facing);
  if (sheetPath.empty()) {
    return;
  }

  // Load/get the texture
  SDL_Texture* tex = sprites_.get(sheetPath);
  if (tex == nullptr) {
    return;
  }

  // Calculate current frame based on time and fps
  int frameIndex = 0;
  if (clip.fps > 0.0F && clip.frames > 1) {
    float animDuration = static_cast<float>(clip.frames) / clip.fps;
    float t = std::fmod(simTime, animDuration);
    frameIndex = static_cast<int>(t * clip.fps);
    frameIndex = std::clamp(frameIndex, 0, clip.frames - 1);
  }

  // Source rectangle in sprite sheet
  const int frameW = cfg.render.frameW;
  const int frameH = cfg.render.frameH;
  SDL_FRect srcRect{static_cast<float>((clip.start + frameIndex) * frameW),
                    static_cast<float>(clip.row * frameH), static_cast<float>(frameW),
                    static_cast<float>(frameH)};

  // Destination rectangle (centered at posX, bottom at posY)
  const float scale = cfg.render.scale;
  const float dstW = static_cast<float>(frameW) * scale;
  const float dstH = static_cast<float>(frameH) * scale;
  const float dstX = posX - dstW * 0.5F + cfg.render.offsetX - camX_;
  const float dstY = posY - dstH + cfg.render.offsetY - camY_;
  SDL_FRect dstRect{dstX, dstY, dstW, dstH};

  // Flip horizontally if using single sheet and facing left
  SDL_FlipMode flip = SDL_FLIP_NONE;
  if (!cfg.render.hasDirectionalSheets() && facing < 0) {
    flip = SDL_FLIP_HORIZONTAL;
  }

  // Render with rotation and flip
  (void)SDL_RenderTextureRotated(renderer_, tex, &srcRect, &dstRect,
                                 static_cast<double>(clip.rotateDeg), nullptr, flip);
}

void App::renderEnemySprite(EntityId id,
                            const EnemyConfig& cfg,
                            int facing,
                            float posX,
                            float posY) {
  // Get animation state for this enemy
  auto* animStatePtr = world_.registry.try_get<EnemyAnimState>(id);
  EnemyAnimState animState{};
  if (animStatePtr != nullptr) {
    animState = *animStatePtr;
  }

  // Get velocity to determine animation
  std::string animName = "idle";
  auto* velPtr = world_.registry.try_get<Velocity>(id);
  if (velPtr != nullptr) {
    const float vx = std::fabs(velPtr->v.x);
    if (vx > 0.1F) {
      animName = "walk";
    }
  }

  // Get the animation clip
  auto itClip = cfg.render.anims.find(animName);
  if (itClip == cfg.render.anims.end()) {
    // Fallback to idle if walk doesn't exist
    itClip = cfg.render.anims.find("idle");
    if (itClip == cfg.render.anims.end()) {
      return;
    }
  }
  const EnemyConfig::RenderClip& clip = itClip->second;

  // Get the sprite sheet for this facing direction
  const std::string sheetPath = cfg.render.getSheet(facing);
  if (sheetPath.empty()) {
    return;
  }

  // Load/get the texture
  SDL_Texture* tex = sprites_.get(sheetPath);
  if (tex == nullptr) {
    return;
  }

  // Calculate current frame based on time and fps
  int frameIndex = 0;
  if (clip.fps > 0.0F && clip.frames > 1) {
    // Use a simple time-based approach (global sim time)
    float animDuration = static_cast<float>(clip.frames) / clip.fps;
    float t = std::fmod(animState.timer, animDuration);
    frameIndex = static_cast<int>(t * clip.fps);
    frameIndex = std::clamp(frameIndex, 0, clip.frames - 1);
  }

  // Source rectangle in sprite sheet
  const int frameW = cfg.render.frameW;
  const int frameH = cfg.render.frameH;
  SDL_FRect srcRect{static_cast<float>((clip.start + frameIndex) * frameW),
                    static_cast<float>(clip.row * frameH), static_cast<float>(frameW),
                    static_cast<float>(frameH)};

  // Destination rectangle (centered at posX, bottom at posY)
  const float scale = cfg.render.scale;
  const float dstW = static_cast<float>(frameW) * scale;
  const float dstH = static_cast<float>(frameH) * scale;
  const float dstX = posX - dstW * 0.5F + cfg.render.offsetX - camX_;
  const float dstY = posY - dstH + cfg.render.offsetY - camY_;
  SDL_FRect dstRect{dstX, dstY, dstW, dstH};

  // Render the sprite (directional sheets mean no flip needed)
  (void)SDL_RenderTexture(renderer_, tex, &srcRect, &dstRect);
}
