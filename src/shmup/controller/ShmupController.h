#pragma once

#include <string>
#include <vector>

#include "ecs/Entity.h"
#include "shmup/systems/ShmupSystems.h"

class World;
struct TimeStep;

namespace shmup {

struct ShipConfig;
struct WeaponConfig;
struct SatelliteConfig;
struct LevelConfig;

// Runtime state for SHMUP game mode.
// Manages player ship, satellites, weapons, and game state.
class ShmupController {
 public:
  // Spawn the player ship with the given config.
  // Loads weapon and satellite configs as needed.
  EntityId spawnPlayer(World& world, const ShipConfig& shipCfg, float startX, float startY);

  // Spawn a satellite attached to the player.
  EntityId spawnSatellite(World& world,
                          EntityId owner,
                          const SatelliteConfig& satCfg,
                          int slotIndex);

  // Load level configuration for wave spawning.
  void loadLevel(const LevelConfig* levelCfg);

  // Update all SHMUP-specific systems for one frame.
  void update(World& world, TimeStep ts);

  // Accessors
  [[nodiscard]] EntityId player() const { return player_; }
  [[nodiscard]] const std::vector<EntityId>& satellites() const { return satellites_; }
  [[nodiscard]] float scrollX() const { return scrollX_; }
  [[nodiscard]] bool isPaused() const { return paused_; }

  void setPaused(bool p) { paused_ = p; }
  void setScrollSpeed(float speed) { scrollSpeed_ = speed; }
  void setLives(int l) { lives_ = l; }

 private:
  EntityId player_ = kInvalidEntity;
  std::vector<EntityId> satellites_;

  // Scroll state
  float scrollX_ = 0.0F;
  float scrollSpeed_ = 60.0F;  // pixels per second
  bool paused_ = false;

  // Wave spawning
  WaveSpawner waveSpawner_;

  // Game state
  int lives_ = 3;
};

}  // namespace shmup
