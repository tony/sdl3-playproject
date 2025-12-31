#pragma once

#include <SDL3/SDL_render.h>

#include <memory>
#include <string>

#include "core/Input.h"
#include "core/SpriteCache.h"
#include "core/Time.h"
#include "ecs/World.h"
#include "shmup/controller/ShmupController.h"

namespace shmup {

// Top-level game mode orchestration for SHMUP gameplay.
// Manages game state, input, update loop, and rendering.
class ShmupGame {
 public:
  struct Config {
    const char* shipId = "fighter";
    const char* stageId = "stage1";
    float spawnX = 100.0F;
    float spawnY = 360.0F;
  };

  ShmupGame();
  ~ShmupGame();

  // Initialize the game with a renderer and config
  bool init(SDL_Renderer* renderer, const Config& cfg);

  // Process a frame: update input, simulation, render
  void tick(TimeStep ts, const InputState& input);

  // Render the current game state
  void render(SDL_Renderer* renderer, int viewW, int viewH);

  // Check if game is still running (not game over)
  [[nodiscard]] bool isRunning() const { return running_; }

  // Reset to initial state
  void reset();

  // Accessors for debug/testing
  [[nodiscard]] const World& world() const { return world_; }
  [[nodiscard]] EntityId player() const { return controller_.player(); }
  [[nodiscard]] int score() const { return score_; }
  [[nodiscard]] int lives() const { return lives_; }
  [[nodiscard]] float scrollX() const { return controller_.scrollX(); }

 private:
  void spawnPlayerShip();
  void updateInput(const InputState& input);
  void renderBackground(SDL_Renderer* renderer, int viewW, int viewH);
  void renderEntities(SDL_Renderer* renderer);
  void renderEffects(SDL_Renderer* renderer);
  void renderHUD(SDL_Renderer* renderer, int viewW, int viewH);

  SDL_Renderer* renderer_ = nullptr;
  SpriteCache spriteCache_;
  World world_;
  ShmupController controller_;

  std::string shipId_;
  std::string stageId_;
  float spawnX_ = 100.0F;
  float spawnY_ = 360.0F;

  bool running_ = true;
  bool paused_ = false;
  int score_ = 0;
  int lives_ = 3;

  float backgroundX_ = 0.0F;
  static constexpr float kBackgroundScrollSpeed = 30.0F;  // px/sec
};

}  // namespace shmup
