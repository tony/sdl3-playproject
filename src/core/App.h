#pragma once

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "character/CharacterConfig.h"
#include "core/DebugUI.h"
#include "core/Input.h"
#include "core/InputScript.h"
#include "core/SpriteCache.h"
#include "core/Time.h"
#include "ecs/Components.h"
#include "ecs/Entity.h"
#include "ecs/World.h"
#include "stage/Stage.h"
#include "visual/Form.h"
#include "visual/Shapes.h"

struct AppConfig {
  const char* stageTomlPath = nullptr;
  const char* characterTomlPath = nullptr;
  const char* spawnPoint = nullptr;
  const char* inputScriptTomlPath = nullptr;
  float spawnX = 120.0F;
  float spawnY = 120.0F;
  bool spawnOverride = false;
  bool noPrefs = false;
  bool noUi = false;
  bool noRender = false;
};

class App {
 public:
  struct PlayerSnapshot {
    bool valid = false;
    float x = 0.0F;
    float y = 0.0F;
    float vx = 0.0F;
    float vy = 0.0F;
    float camX = 0.0F;
    float camY = 0.0F;
    bool grounded = false;
    int respawns = 0;
    int hurtEvents = 0;
    int enemyKills = 0;
    bool cameraBackscrolled = false;
  };

  bool init(const AppConfig& cfg);
  void run(int maxFrames = -1);
  void shutdown();
  bool playerGrounded() const;
  int respawnCount() const;
  bool cameraBackscrolled() const { return cameraBackscrolled_; }
  bool playerSnapshot(PlayerSnapshot& out) const;

  const std::string& stagePath() const { return stagePath_; }
  const std::string& characterPath() const { return characterPath_; }
  const std::string& spawnPoint() const { return spawnPoint_; }
  float spawnX() const { return spawnX_; }
  float spawnY() const { return spawnY_; }
  bool hasInputScript() const { return inputScriptEnabled_; }
  const std::string& inputScriptPath() const { return inputScript_.path(); }

 private:
  void handleEvent(const SDL_Event& e);
  void handleCommands(const AppCommands& cmds);
  void tick(TimeStep ts);
  void render();
  void updateCamera(int viewW, int viewH);
  void updateWindowResizeCursors(int winW, int winH);
  void renderDebugCamera(int viewW, int viewH);
  void renderDebugOverlay();

  void respawnPlayer();
  void respawnEnemies();
  void respawnCollectibles();
  void teleportPlayerToSpawn();
  bool reloadStageFromDisk();
  bool reloadCharacterFromDiskInPlace(bool resetRuntime);
  bool swapCharacterInPlace(const std::string& nextPath, bool resetRuntime);
  void startCharacterSwapZap();
  void renderCharacterSwapZap();
  void resetPlayerState();
  void refreshMenuLists();
  void refreshSpawnLists();
  void syncWatchedFiles();
  void maybeAutoReload();
  void rebuildCharacterForms();
  void updateCameraLook(TimeStep ts);
  void applyKillPlane();
  void resetCameraTracking();
  int resolveCameraFacing(EntityId id) const;
  bool resolveCameraTarget(float& targetX,
                           float& targetY,
                           Rect& playerAabb,
                           float& boxW,
                           float& boxH);
  void applyCameraDeadzone(bool shouldSnap,
                           float targetX,
                           float targetY,
                           float viewHalfW,
                           float viewHalfH,
                           float& desiredX,
                           float& desiredY);
  void renderCharacterSprite(EntityId id,
                             const struct CharacterConfig& cfg,
                             AnimStateId animId,
                             int facing,
                             float posX,
                             float posY,
                             float simTime);
  void renderEnemySprite(EntityId id,
                         const struct EnemyConfig& cfg,
                         int facing,
                         float posX,
                         float posY);

  SDL_Window* window_ = nullptr;
  SDL_Renderer* renderer_ = nullptr;
  SDL_Texture* gameTarget_ = nullptr;
  int gameTargetW_ = 0;
  int gameTargetH_ = 0;
  int lastGameAvailW_ = 0;
  int lastGameAvailH_ = 0;
  bool running_ = true;

  SDL_Cursor* cursorDefault_ = nullptr;
  SDL_Cursor* cursorNwResize_ = nullptr;
  SDL_Cursor* cursorNeResize_ = nullptr;
  SDL_Cursor* cursorSeResize_ = nullptr;
  SDL_Cursor* cursorSwResize_ = nullptr;

  bool debugOverlay_ = false;
  bool debugCollision_ = false;
  bool panelsOpen_ = true;
  bool uiCaptureKeyboard_ = false;
  bool uiCaptureMouse_ = false;
  bool simPaused_ = false;
  int pendingSimSteps_ = 0;
  float timeScale_ = 1.0F;
  int internalResMode_ = 0;  // 0=auto, then fixed modes in App::render
  bool integerScaleOnly_ = false;
  int gamepadDeadzone_ = 8000;
  bool autoReload_ = false;
  bool uiEnabled_ = true;
  bool renderEnabled_ = true;
  bool prefsEnabled_ = true;

  TimeStep lastTs_{};
  uint64_t simFrame_ = 0;
  float camX_ = 0.0F;
  float camY_ = 0.0F;
  bool cameraSnapPending_ = true;  // Snap camera to player on next update
  bool cameraBackscrolled_ = false;
  bool cameraTrackValid_ = false;
  float lastTrackedCamX_ = 0.0F;
  float cameraLookOffsetY_ = 0.0F;
  int lookUpHeldMs_ = 0;
  int lookDownHeldMs_ = 0;
  int respawnCount_ = 0;

  std::vector<std::string> menuStages_;
  std::vector<std::string> menuStageLabels_;
  std::vector<std::string> menuCharacters_;
  std::vector<std::string> menuCharacterLabels_;
  std::vector<std::string> menuSpawns_;
  std::vector<DebugUIStageInspectorModel::SpawnInfo> menuSpawnInfos_;

  std::string stagePath_;
  std::string characterPath_;
  std::string formPath_;
  std::string spawnPoint_;  // empty = custom spawnX_/spawnY_
  float spawnX_ = 120.0F;
  float spawnY_ = 120.0F;

  uint64_t lastAutoReloadCheckNs_ = 0;
  std::filesystem::file_time_type stageMtime_;
  std::filesystem::file_time_type characterMtime_;
  std::filesystem::file_time_type formMtime_;
  bool stageMtimeValid_ = false;
  bool characterMtimeValid_ = false;
  bool formMtimeValid_ = false;

  Input input_;
  DebugUI debugUi_;
  SpriteCache sprites_;
  InputScript inputScript_;
  bool inputScriptEnabled_ = false;
  World world_;
  Stage stage_;

  // Visual system
  std::unique_ptr<Visual::ShapeRenderer> shapeRenderer_;
  std::unique_ptr<Visual::FormRenderer> formRenderer_;
  Visual::CharacterForm puppetForm_;
  Visual::CharacterForm fallbackForm_;

  std::unordered_map<EntityId, Visual::FormRuntime> formRuntime_;

  // Afterimage tracking (recent positions for motion effects)
  struct AfterimageFrame {
    Visual::Vec2 pos{0, 0};
    float facing = 1.0F;
    Visual::Pose pose;
    float age = 0.0F;  // seconds since recorded
  };
  static constexpr int kMaxAfterimages = 4;
  std::array<AfterimageFrame, kMaxAfterimages> afterimages_{};
  int afterimageIndex_ = 0;
  float afterimageTimer_ = 0.0F;

  float characterSwapZapTimer_ = 0.0F;
  float characterSwapZapDuration_ = 0.28F;
  Visual::Vec2 characterSwapZapPos_{0.0F, 0.0F};
};
