#pragma once

#include "ecs/Components.h"
#include "shmup/config/WeaponConfig.h"

class World;

namespace shmup {

// Fire pattern algorithms for spawning projectiles.
namespace FirePatterns {

// Spawn projectiles based on weapon config and level stats.
// @param w          The world to spawn into
// @param cfg        Weapon configuration
// @param stats      Level-specific stats (projectile count, spread, damage, etc.)
// @param firePos    World position to spawn from
// @param baseAngle  Base firing angle (radians, 0 = right, pi/2 = down)
// @param fromPlayer True if fired by player, false if by enemy
void fire(World& w, const WeaponConfig& cfg, const WeaponConfig::LevelStats& stats, Vec2 firePos,
          float baseAngle, bool fromPlayer);

// Pattern-specific spawn functions
void fireSingle(World& w, const WeaponConfig& cfg, const WeaponConfig::LevelStats& stats,
                Vec2 firePos, float baseAngle, bool fromPlayer);
void fireSpread(World& w, const WeaponConfig& cfg, const WeaponConfig::LevelStats& stats,
                Vec2 firePos, float baseAngle, bool fromPlayer);
void fireHoming(World& w, const WeaponConfig& cfg, const WeaponConfig::LevelStats& stats,
                Vec2 firePos, float baseAngle, bool fromPlayer);
void fireWave(World& w, const WeaponConfig& cfg, const WeaponConfig::LevelStats& stats,
              Vec2 firePos, float baseAngle, bool fromPlayer);

}  // namespace FirePatterns

}  // namespace shmup
