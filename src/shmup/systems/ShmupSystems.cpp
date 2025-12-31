#include "shmup/systems/ShmupSystems.h"

#include <algorithm>
#include <cmath>

#include "core/Time.h"
#include "ecs/Components.h"
#include "ecs/World.h"
#include "shmup/components/ShmupComponents.h"
#include "shmup/config/WeaponConfig.h"
#include "shmup/controller/FirePatterns.h"

namespace shmup {

namespace {

constexpr float kPi = 3.14159265358979323846F;

float lerp(float a, float b, float t) {
  return a + (b - a) * t;
}

// Smoothly interpolate between two values over time
float lerpSmooth(float current, float target, float smoothing, float dt) {
  return lerp(current, target, 1.0F - std::exp(-smoothing * dt));
}

}  // namespace

void ShmupSystems::playerMovement(World& w, [[maybe_unused]] TimeStep ts) {
  auto view = w.registry.view<ShmupPlayerTag, ShipState, Transform, Velocity, InputState>();

  for (auto [entity, ship, t, vel, input] : view.each()) {
    // Focus mode: action2 (typically shift key)
    ship.focused = input.action2Held;

    // Select speed based on focus mode
    const float speed = ship.focused ? ship.focusedSpeed : ship.speed;

    // Calculate desired velocity from input
    float dx = 0.0F;
    float dy = 0.0F;
    if (input.left) dx -= 1.0F;
    if (input.right) dx += 1.0F;
    if (input.upHeld) dy -= 1.0F;
    if (input.downHeld) dy += 1.0F;

    // Normalize diagonal movement
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len > 0.0F) {
      dx /= len;
      dy /= len;
    }

    if (ship.instantMovement) {
      // Instant movement (typical for SHMUPs)
      vel.v.x = dx * speed;
      vel.v.y = dy * speed;
    } else {
      // Smooth acceleration (for some ship types)
      constexpr float accel = 0.15F;
      vel.v.x = lerp(vel.v.x, dx * speed, accel);
      vel.v.y = lerp(vel.v.y, dy * speed, accel);
    }

    // Apply velocity to position
    t.pos.x += vel.v.x;
    t.pos.y += vel.v.y;

    // Clamp to screen bounds (assuming 1280x720 for now)
    // TODO: Get actual screen dimensions from game state
    constexpr float kMargin = 16.0F;
    constexpr float kScreenW = 1280.0F;
    constexpr float kScreenH = 720.0F;
    t.pos.x = std::clamp(t.pos.x, kMargin, kScreenW - kMargin);
    t.pos.y = std::clamp(t.pos.y, kMargin, kScreenH - kMargin);
  }
}

void ShmupSystems::satelliteUpdate(World& w) {
  auto view = w.registry.view<ShmupSatelliteTag, SatelliteState, Transform>();

  for (auto [entity, sat, t] : view.each()) {
    // Get owner's transform and ship state
    if (!w.registry.valid(sat.owner)) {
      continue;
    }

    const auto& ownerT = w.registry.get<Transform>(sat.owner);
    const auto* ownerShip = w.registry.try_get<ShipState>(sat.owner);
    const bool focused = (ownerShip != nullptr) && ownerShip->focused;

    // Update orbit angle (always spinning, even if not using orbit mode)
    sat.orbitAngle += sat.orbitSpeed;
    if (sat.orbitAngle > 2.0F * kPi) {
      sat.orbitAngle -= 2.0F * kPi;
    }

    // Calculate position based on mode
    float offsetX = 0.0F;
    float offsetY = 0.0F;

    switch (sat.positionMode) {
      case SatelliteState::PositionMode::Fixed:
        offsetX = focused ? sat.focusedOffsetX : sat.fixedOffsetX;
        offsetY = focused ? sat.focusedOffsetY : sat.fixedOffsetY;
        break;

      case SatelliteState::PositionMode::Orbit: {
        const float radius = focused ? sat.focusedRadius : sat.orbitRadius;
        offsetX = std::cos(sat.orbitAngle) * radius;
        offsetY = std::sin(sat.orbitAngle) * radius;
      } break;

      case SatelliteState::PositionMode::Formation:
        // Formation mode: lerp between normal and focused positions
        if (focused) {
          offsetX = sat.focusedOffsetX;
          offsetY = sat.focusedOffsetY;
        } else {
          offsetX = sat.fixedOffsetX;
          offsetY = sat.fixedOffsetY;
        }
        break;
    }

    // Smoothly transition to target position
    constexpr float kSmoothing = 12.0F;
    constexpr float kDt = 1.0F / 60.0F;  // Assume 60fps for smoothing
    sat.currentOffsetX = lerpSmooth(sat.currentOffsetX, offsetX, kSmoothing, kDt);
    sat.currentOffsetY = lerpSmooth(sat.currentOffsetY, offsetY, kSmoothing, kDt);

    t.pos.x = ownerT.pos.x + sat.currentOffsetX;
    t.pos.y = ownerT.pos.y + sat.currentOffsetY;
  }
}

void ShmupSystems::weaponFiring(World& w, [[maybe_unused]] TimeStep ts) {
  // Handle player weapon firing
  auto playerView = w.registry.view<ShmupPlayerTag, WeaponState, InputState, Transform>();

  for (auto [entity, weapons, input, t] : playerView.each()) {
    weapons.firing = input.action1Held;

    for (auto& mount : weapons.mounts) {
      // Decrement cooldown
      if (mount.cooldownFrames > 0) {
        --mount.cooldownFrames;
      }

      if (weapons.firing && mount.cooldownFrames <= 0) {
        const WeaponConfig* cfg = WeaponRegistry::get(mount.weaponId);
        if (cfg != nullptr) {
          const auto& stats = cfg->getLevel(mount.level);

          // Fire projectiles using pattern
          Vec2 firePos{t.pos.x + mount.offsetX, t.pos.y + mount.offsetY};
          FirePatterns::fire(w, *cfg, stats, firePos, 0.0F, true);

          // Set cooldown
          mount.cooldownFrames = stats.cooldownFrames;
        }
      }
    }
  }

  // Handle satellite weapon firing (inherit from owner)
  auto satView = w.registry.view<ShmupSatelliteTag, SatelliteState, Transform>();

  for (auto [entity, sat, t] : satView.each()) {
    if (!w.registry.valid(sat.owner)) {
      continue;
    }

    const auto* ownerWeapons = w.registry.try_get<WeaponState>(sat.owner);
    if ((ownerWeapons == nullptr) || !ownerWeapons->firing) {
      continue;
    }

    // Satellites fire with same cooldown timing as owner's first weapon
    // TODO: Support satellite-specific weapon overrides
    if (!ownerWeapons->mounts.empty()) {
      const auto& ownerMount = ownerWeapons->mounts[0];
      // Check if owner just fired (cooldown was just reset)
      const WeaponConfig* cfg = WeaponRegistry::get(ownerMount.weaponId);
      if (cfg != nullptr) {
        const auto& stats = cfg->getLevel(ownerMount.level);
        // Fire when owner's cooldown matches max (just reset)
        if (ownerMount.cooldownFrames == stats.cooldownFrames) {
          FirePatterns::fire(w, *cfg, stats, t.pos, 0.0F, true);
        }
      }
    }
  }
}

void ShmupSystems::projectiles(World& w, [[maybe_unused]] TimeStep ts) {
  auto view = w.registry.view<ShmupProjectileTag, ShmupProjectileState, Transform, Velocity>();

  std::vector<EntityId> toRemove;

  for (auto [entity, proj, t, vel] : view.each()) {
    // Apply velocity
    t.pos.x += vel.v.x;
    t.pos.y += vel.v.y;

    // Apply gravity if any
    vel.v.y += proj.gravity;

    // Handle homing
    if (w.registry.all_of<HomingState>(entity)) {
      auto& homing = w.registry.get<HomingState>(entity);
      if (homing.delayFrames > 0) {
        --homing.delayFrames;
      } else {
        // Find closest valid target
        EntityId closestTarget = kInvalidEntity;
        float closestDist = homing.seekRadius * homing.seekRadius;

        if (proj.fromPlayer) {
          // Player projectiles seek enemies
          auto enemyView = w.registry.view<ShmupEnemyTag, Transform>();
          for (auto [enemyEnt, enemyT] : enemyView.each()) {
            const float dx = enemyT.pos.x - t.pos.x;
            const float dy = enemyT.pos.y - t.pos.y;
            const float distSq = dx * dx + dy * dy;
            if (distSq < closestDist) {
              closestDist = distSq;
              closestTarget = enemyEnt;
            }
          }
        } else {
          // Enemy projectiles seek player
          if (w.player != kInvalidEntity && w.registry.valid(w.player)) {
            const auto& playerT = w.registry.get<Transform>(w.player);
            const float dx = playerT.pos.x - t.pos.x;
            const float dy = playerT.pos.y - t.pos.y;
            const float distSq = dx * dx + dy * dy;
            if (distSq < closestDist) {
              closestTarget = w.player;
            }
          }
        }

        // Turn toward target
        if (closestTarget != kInvalidEntity) {
          const auto& targetT = w.registry.get<Transform>(closestTarget);
          const float dx = targetT.pos.x - t.pos.x;
          const float dy = targetT.pos.y - t.pos.y;
          float targetAngle = std::atan2(dy, dx);
          float currentAngle = std::atan2(vel.v.y, vel.v.x);

          // Calculate angle difference
          float diff = targetAngle - currentAngle;
          while (diff > kPi) diff -= 2.0F * kPi;
          while (diff < -kPi) diff += 2.0F * kPi;

          // Limit turn rate
          diff = std::clamp(diff, -homing.turnRate, homing.turnRate);

          // Apply turn
          float newAngle = currentAngle + diff;
          float speed = std::sqrt(vel.v.x * vel.v.x + vel.v.y * vel.v.y);
          vel.v.x = std::cos(newAngle) * speed;
          vel.v.y = std::sin(newAngle) * speed;
        }
      }
    }

    // Decrement lifetime
    if (proj.lifetimeFrames > 0) {
      --proj.lifetimeFrames;
      if (proj.lifetimeFrames <= 0) {
        toRemove.push_back(entity);
        continue;
      }
    }

    // Check screen bounds (with margin for off-screen removal)
    constexpr float kMargin = 64.0F;
    constexpr float kScreenW = 1280.0F;
    constexpr float kScreenH = 720.0F;
    if (t.pos.x < -kMargin || t.pos.x > kScreenW + kMargin || t.pos.y < -kMargin ||
        t.pos.y > kScreenH + kMargin) {
      toRemove.push_back(entity);
    }
  }

  for (EntityId e : toRemove) {
    w.destroy(e);
  }
}

void ShmupSystems::collision(World& w) {
  // Player projectiles vs enemies
  auto projView = w.registry.view<ShmupProjectileTag, ShmupProjectileState, Transform, AABB>();
  auto enemyView = w.registry.view<ShmupEnemyTag, ShmupEnemyState, Transform, AABB>();

  std::vector<EntityId> projToRemove;
  std::vector<EntityId> enemiesToRemove;

  for (auto [projEnt, proj, projT, projAABB] : projView.each()) {
    if (!proj.fromPlayer) continue;

    for (auto [enemyEnt, enemy, enemyT, enemyAABB] : enemyView.each()) {
      if (enemy.invulnerable) continue;

      // Simple AABB collision
      const float dx = std::abs(projT.pos.x - enemyT.pos.x);
      const float dy = std::abs(projT.pos.y - enemyT.pos.y);
      const float overlapX = (projAABB.w + enemyAABB.w) * 0.5F;
      const float overlapY = (projAABB.h + enemyAABB.h) * 0.5F;

      if (dx < overlapX && dy < overlapY) {
        // Hit! Apply damage
        enemy.health -= static_cast<int>(proj.damage);
        --proj.pierceRemaining;

        if (enemy.health <= 0) {
          enemiesToRemove.push_back(enemyEnt);
        }

        if (proj.pierceRemaining <= 0) {
          projToRemove.push_back(projEnt);
          break;  // Projectile is done
        }
      }
    }
  }

  // Enemy projectiles vs player
  auto playerView = w.registry.view<ShmupPlayerTag, ShipState, Transform, AABB>();
  for (auto [projEnt, proj, projT, projAABB] : projView.each()) {
    if (proj.fromPlayer) continue;

    for (auto [playerEnt, ship, playerT, playerAABB] : playerView.each()) {
      if (ship.invincibleFrames > 0) continue;

      const float dx = std::abs(projT.pos.x - playerT.pos.x);
      const float dy = std::abs(projT.pos.y - playerT.pos.y);
      const float overlapX = (projAABB.w + playerAABB.w) * 0.5F;
      const float overlapY = (projAABB.h + playerAABB.h) * 0.5F;

      if (dx < overlapX && dy < overlapY) {
        // Player hit!
        --ship.health;
        ship.invincibleFrames = 120;  // 2 seconds at 60fps
        projToRemove.push_back(projEnt);
        break;
      }
    }
  }

  for (EntityId e : projToRemove) {
    if (w.registry.valid(e)) {
      w.destroy(e);
    }
  }
  for (EntityId e : enemiesToRemove) {
    if (w.registry.valid(e)) {
      w.destroy(e);
      ++w.enemyKills;
    }
  }
}

void ShmupSystems::cleanup(World& w) {
  // Decrement player invincibility
  auto playerView = w.registry.view<ShmupPlayerTag, ShipState>();
  for (auto [entity, ship] : playerView.each()) {
    if (ship.invincibleFrames > 0) {
      --ship.invincibleFrames;
    }
  }
}

}  // namespace shmup
