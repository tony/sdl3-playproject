#pragma once

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/SpriteCache.h"

// ImGui integration is optional and should remain a core-only, debug-only layer.
// Builds without ImGui support provide a no-op implementation.

struct DebugUIVisualRulesModel {
  // Palette generation
  bool highlightWarm = true;
  bool shadowCool = true;

  // Form rendering
  float outlineScale = 1.12F;  // how much larger outline shapes are

  // Motion
  float squashStretchMax = 1.3F;  // maximum distortion factor
  bool preserveMass = true;       // width * height stays constant
  float smearThreshold = 400.0F;  // velocity before smear kicks in
  int afterimageCount = 3;
  float afterimageDecay = 0.25F;  // seconds
};

struct DebugUIMenuModel {
  std::string stagePath;
  std::string characterPath;
  std::string spawnPoint;  // empty = custom spawnX/spawnY
  float spawnX = 0.0F;
  float spawnY = 0.0F;
  const std::vector<std::string>* stagePaths = nullptr;
  const std::vector<std::string>* stageLabels = nullptr;
  const std::vector<std::string>* characterPaths = nullptr;
  const std::vector<std::string>* characterLabels = nullptr;
  const std::vector<std::string>* spawnPoints = nullptr;

  bool autoReload = false;
  bool simPaused = false;
  float timeScale = 1.0F;
  int internalResMode = 0;
  bool integerScaleOnly = false;
  int gamepadDeadzone = 8000;
  DebugUIVisualRulesModel visualRules{};
};

struct DebugUIMenuActions {
  bool close = false;
  bool quit = false;
  bool resetLayoutAndPrefs = false;
  bool resetVisualRules = false;
  bool respawn = false;
  bool reloadStage = false;
  bool reloadCharacter = false;
  bool reloadCharacterInPlace = false;
  bool reloadCharacterResetRuntime = false;
  bool teleportToSpawn = false;
  bool resetState = false;

  bool selectStage = false;
  std::string stagePath;

  bool selectCharacter = false;
  std::string characterPath;

  bool swapCharacterInPlace = false;
  std::string swapCharacterPath;

  bool selectSpawnPoint = false;
  std::string spawnPoint;

  bool setSimPaused = false;
  bool simPaused = false;
  int stepFrames = 0;

  bool setAutoReload = false;
  bool autoReload = false;

  bool setTimeScale = false;
  float timeScale = 1.0F;

  bool setInternalResMode = false;
  int internalResMode = 0;

  bool setIntegerScaleOnly = false;
  bool integerScaleOnly = false;

  bool setGamepadDeadzone = false;
  int gamepadDeadzone = 8000;

  bool setVisualRules = false;
  DebugUIVisualRulesModel visualRules{};
};

struct DebugUIOverlayModel {
  uint64_t frame = 0;
  float dt = 0.0F;
  bool debugOverlay = false;
  bool debugCollision = false;
  float camX = 0.0F;
  float camY = 0.0F;

  bool hasPlayer = false;
  uint32_t playerId = 0;
  std::string playerName;
  bool hasPos = false;
  float posX = 0.0F;
  float posY = 0.0F;
  bool hasVel = false;
  float velX = 0.0F;
  float velY = 0.0F;
  bool hasGrounded = false;
  bool grounded = false;

  bool hasAabb = false;
  float aabbW = 0.0F;
  float aabbH = 0.0F;

  bool hasInput = false;
  bool dashHeld = false;

  bool hasTimers = false;
  int coyoteFrames = 0;
  int jumpBufferFrames = 0;
  int actionCooldownFrames = 0;
  int attackCooldownFrames = 0;

  bool hasActionState = false;
  int dashRemainingFrames = 0;
  int airDashesUsed = 0;
  bool spindashCharging = false;
  int spindashChargeFrames = 0;
  bool spinning = false;
  int spinFacingX = 1;
  bool flying = false;
  bool gliding = false;
  bool walling = false;
  int wallDirX = 0;
  int dashWallKickFrames = 0;
};

struct DebugUIStageInspectorModel {
  std::string stagePath;
  int stageVersion = 0;
  std::string stageId;
  std::string stageDisplay;
  std::string renderBgTopHex;
  std::string renderBgBottomHex;
  std::string renderPlatformBaseHex;
  std::string renderPlatformLightHex;
  std::string renderPlatformDarkHex;
  std::string renderPlatformHighlightHex;
  std::size_t solidCount = 0;
  std::size_t slopeCount = 0;
  float collisionGroundSnap = 0.0F;
  float collisionStepUp = 0.0F;
  float collisionSkin = 0.0F;
  const std::vector<std::string>* spawnPoints = nullptr;
  struct SpawnInfo {
    std::string name;
    float x = 0.0F;
    float y = 0.0F;
    int facingX = 1;
  };
  const std::vector<SpawnInfo>* spawns = nullptr;

  bool hasWorldBounds = false;
  float worldX = 0.0F;
  float worldY = 0.0F;
  float worldW = 0.0F;
  float worldH = 0.0F;

  bool hasCameraBounds = false;
  float cameraX = 0.0F;
  float cameraY = 0.0F;
  float cameraW = 0.0F;
  float cameraH = 0.0F;

  bool hasCameraFollow = false;
  float cameraDeadzoneW = 0.0F;
  float cameraDeadzoneH = 0.0F;
  float cameraLookaheadX = 0.0F;
  float cameraLookaheadY = 0.0F;
};

struct DebugUIPlayerInspectorModel {
  DebugUIOverlayModel overlay{};
  std::string animState;
  std::string stageId;
  std::string characterId;
  bool hasFacing = false;
  int facingX = 1;
  bool hasPhysics = false;
  float gravity = 0.0F;
  float fallGravityMultiplier = 1.0F;
  float effectiveGravityMultiplier = 1.0F;
  bool hasMath = false;
  int subpixel = 0;
  std::string movementState;
};

struct DebugUIControlsModel {
  std::string characterLabel;
  std::vector<std::string> legend;
  std::vector<std::string> actions;
};

struct DebugUIPanelsResult {
  SDL_Rect gameViewport{0, 0, 0, 0};
  int gameAvailW = 0;
  int gameAvailH = 0;
  DebugUIMenuActions actions{};
};

class DebugUI {
 public:
  bool init(SDL_Window* window, SDL_Renderer* renderer);
  void shutdown();

  void processEvent(const SDL_Event& e);
  void beginFrame();
  void endFrame(SDL_Renderer* renderer);

  static bool available();
  bool initialized() const { return initialized_; }

  bool wantCaptureKeyboard() const;
  bool wantCaptureMouse() const;

  void drawOverlay(const DebugUIOverlayModel& model);
  void drawStageInspector(const DebugUIStageInspectorModel& model);
  void drawPlayerInspector(const DebugUIPlayerInspectorModel& model);
  DebugUIPanelsResult drawPanels(int windowW,
                                 int windowH,
                                 const DebugUIMenuModel& menuModel,
                                 const DebugUIStageInspectorModel& stageModel,
                                 const DebugUIPlayerInspectorModel& playerModel,
                                 SDL_Texture* gameTexture,
                                 int gameTextureW,
                                 int gameTextureH,
                                 const DebugUIControlsModel& controlsModel);

 private:
  bool initialized_ = false;
  float panelsLeftW_ = 360.0F;
  float panelsRightW_ = 360.0F;
  std::string iniPath_;

  SpriteCache uiSprites_{};
};
