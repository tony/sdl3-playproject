#pragma once

class World;
struct TimeStep;

namespace shmup {

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

// Check collisions: player vs enemy projectiles, player projectiles vs enemies.
// Applies damage and triggers effects.
void collision(World& w);

// Remove expired/off-screen entities.
void cleanup(World& w);

}  // namespace ShmupSystems

}  // namespace shmup
