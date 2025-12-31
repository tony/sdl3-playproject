#pragma once

#include <cstddef>

#include "ecs/Entity.h"

class World;
struct TimeStep;

namespace shmup {

struct LevelConfig;
struct ShmupEnemyConfig;
struct ShmupBossConfig;
struct ShmupItemConfig;

// Runtime wave spawning state
struct WaveSpawner {
  const LevelConfig* level = nullptr;
  std::size_t nextWaveIndex = 0;
  std::size_t nextBossIndex = 0;
  float lastScrollX = 0.0F;

  void init(const LevelConfig* levelCfg);
  void update(World& w, float scrollX);
  void reset();
};

// SHMUP-specific ECS systems.
// These operate on shmup::* components and are called by ShmupController.
namespace ShmupSystems {

// Process player input, update position with focus mode support.
// Uses InputState component: left/right/up/down for movement,
// action1 for fire, action2 for focus (slow mode).
void playerMovement(World& w, TimeStep ts);

// Update satellite positions relative to their owner.
// Handles Fixed, Orbit, and Formation position modes.
// Formation tightens when owner is in focus mode.
void satelliteUpdate(World& w);

// Handle fire button, spawn projectiles based on weapon config.
// Reads WeaponState to get fire patterns and cooldowns.
void weaponFiring(World& w, TimeStep ts);

// Update projectile positions, handle homing logic.
// Removes projectiles that hit boundaries or expire.
void projectiles(World& w, TimeStep ts);

// Update enemy movement patterns (linear, sine, chase, orbit).
void enemyMovement(World& w, TimeStep ts);

// Update enemy firing patterns (aimed, spread, circular).
void enemyFiring(World& w, TimeStep ts);

// Check collisions: player vs enemy projectiles, player projectiles vs enemies.
// Applies damage and triggers effects.
void collision(World& w);

// Remove expired/off-screen entities.
void cleanup(World& w);

// Spawn a single enemy at the given position with config.
EntityId spawnEnemy(World& w, const ShmupEnemyConfig& cfg, float x, float y);

// Spawn a boss at the given position with config.
EntityId spawnBoss(World& w, const ShmupBossConfig& cfg, float x, float y);

// Boss behavior: entrance animation, phase transitions, minion spawning, death.
void boss(World& w, TimeStep ts);

// Spawn a collectible item at the given position with config.
EntityId spawnItem(World& w, const ShmupItemConfig& cfg, float x, float y);

// Process item drops when an enemy dies.
void processEnemyDrop(World& w, const ShmupEnemyConfig& cfg, float x, float y);

// Update item movement (float, bounce, magnetize toward player).
void itemMovement(World& w, TimeStep ts);

// Check player-item collision, apply effects (weapon upgrade, life, etc.).
void itemPickup(World& w);

// Handle player death and respawn with lives system.
// Decrements lives, resets health, grants invincibility, downgrades weapons.
void playerDeath(World& w);

}  // namespace ShmupSystems

}  // namespace shmup
