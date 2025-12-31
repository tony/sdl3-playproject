#pragma once

#include <string>
#include <vector>

#include "ecs/Entity.h"

class World;
struct TimeStep;

namespace shmup {

struct ShipConfig;
struct WeaponConfig;
struct SatelliteConfig;

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

  // Game state
  int score_ = 0;
  int lives_ = 3;
};

}  // namespace shmup
