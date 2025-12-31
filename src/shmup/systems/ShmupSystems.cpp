#include "shmup/systems/ShmupSystems.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "core/Time.h"
#include "ecs/Components.h"
#include "ecs/World.h"
#include "shmup/components/ShmupComponents.h"
#include "shmup/config/LevelConfig.h"
#include "shmup/config/ShmupBossConfig.h"
#include "shmup/config/ShmupEnemyConfig.h"
#include "shmup/config/ShmupItemConfig.h"
#include "shmup/config/WeaponConfig.h"
#include "shmup/controller/FirePatterns.h"

#include <cstdlib>

namespace shmup {

namespace {

// Simple RNG for item drops (0.0 to 1.0)
float randomFloat() {
  return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
}

constexpr float kPi = 3.14159265358979323846F;

float lerp(float a, float b, float t) {
  return a + (b - a) * t;
}

// Smoothly interpolate between two values over time
float lerpSmooth(float current, float target, float smoothing, float dt) {
  return lerp(current, target, 1.0F - std::exp(-smoothing * dt));
}

// Ease-out cubic interpolation for smooth boss entrance
float easeOutCubic(float t) {
  return 1.0F - std::pow(1.0F - t, 3.0F);
}

// Map boss movement type enum to ECS movement type
MovementType mapBossMovementType(ShmupBossConfig::MovementType type) {
  switch (type) {
    case ShmupBossConfig::MovementType::None:
      return MovementType::None;
    case ShmupBossConfig::MovementType::Linear:
      return MovementType::Linear;
    case ShmupBossConfig::MovementType::Sine:
      return MovementType::Sine;
    case ShmupBossConfig::MovementType::Chase:
      return MovementType::Chase;
    case ShmupBossConfig::MovementType::Orbit:
      return MovementType::Orbit;
    case ShmupBossConfig::MovementType::Figure8:
      // Figure8 uses Sine with special handling in boss()
      return MovementType::Sine;
  }
  return MovementType::None;
}

// Map boss fire type enum to ECS fire type
FireType mapBossFireType(ShmupBossConfig::FireType type) {
  switch (type) {
    case ShmupBossConfig::FireType::None:
      return FireType::None;
    case ShmupBossConfig::FireType::Aimed:
      return FireType::Aimed;
    case ShmupBossConfig::FireType::Spread:
      return FireType::Spread;
    case ShmupBossConfig::FireType::Circular:
      return FireType::Circular;
  }
  return FireType::None;
}

// Spawn enemy projectile based on fire pattern
void fireEnemyShot(World& w, EntityId enemy, const Transform& enemyT, Firing& fire) {
  // Calculate aim angle based on pattern type
  float angle = fire.aimAngle;

  if (fire.type == FireType::Aimed && w.player != kInvalidEntity && w.registry.valid(w.player)) {
    const auto& playerT = w.registry.get<Transform>(w.player);
    float dx = playerT.pos.x - enemyT.pos.x;
    float dy = playerT.pos.y - enemyT.pos.y;
    angle = std::atan2(dy, dx);
    fire.aimAngle = angle;
  } else if (fire.type == FireType::Circular) {
    angle = fire.rotationAngle;
    fire.rotationAngle += fire.rotationSpeed;
  }

  // Spawn projectiles
  const float spreadStep =
      (fire.shotCount > 1) ? fire.spreadAngle / static_cast<float>(fire.shotCount - 1) : 0.0F;
  const float startAngle = (fire.shotCount > 1) ? angle - fire.spreadAngle * 0.5F : angle;

  for (int i = 0; i < fire.shotCount; ++i) {
    float shotAngle = startAngle + spreadStep * static_cast<float>(i);

    EntityId id = w.create();
    auto& reg = w.registry;

    reg.emplace<Transform>(id, enemyT.pos);

    constexpr float kDefaultProjectileSpeed = 3.0F;
    Velocity vel;
    vel.v.x = std::cos(shotAngle) * kDefaultProjectileSpeed;
    vel.v.y = std::sin(shotAngle) * kDefaultProjectileSpeed;
    reg.emplace<Velocity>(id, vel);

    reg.emplace<AABB>(id, 8.0F, 8.0F);
    reg.emplace<ShmupProjectileTag>(id);

    ShmupProjectileState proj;
    proj.damage = 1.0F;
    proj.lifetimeFrames = 180;
    proj.pierceRemaining = 1;
    proj.fromPlayer = false;
    proj.owner = enemy;
    proj.gravity = 0.0F;
    reg.emplace<ShmupProjectileState>(id, proj);
  }
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
    if (input.left)
      dx -= 1.0F;
    if (input.right)
      dx += 1.0F;
    if (input.upHeld)
      dy -= 1.0F;
    if (input.downHeld)
      dy += 1.0F;

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

    // Clamp to screen bounds (hardcoded 1280x720 for now)
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
          while (diff > kPi)
            diff -= 2.0F * kPi;
          while (diff < -kPi)
            diff += 2.0F * kPi;

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
    if (!proj.fromPlayer)
      continue;

    for (auto [enemyEnt, enemy, enemyT, enemyAABB] : enemyView.each()) {
      if (enemy.invulnerable)
        continue;

      // Simple AABB collision
      const float dx = std::abs(projT.pos.x - enemyT.pos.x);
      const float dy = std::abs(projT.pos.y - enemyT.pos.y);
      const float overlapX = (projAABB.w + enemyAABB.w) * 0.5F;
      const float overlapY = (projAABB.h + enemyAABB.h) * 0.5F;

      if (dx < overlapX && dy < overlapY) {
        // Hit! Apply damage
        enemy.health -= static_cast<int>(proj.damage);
        --proj.pierceRemaining;

        // Spawn hit spark at impact point
        spawnHitSpark(w, projT.pos.x, projT.pos.y, 0.0F);

        if (enemy.health <= 0) {
          // Add score from enemy
          w.score += enemy.scoreValue;

          // Spawn explosion at enemy position
          spawnExplosion(w, enemyT.pos.x, enemyT.pos.y, 1.0F);

          // Process item drops before removing enemy
          const ShmupEnemyConfig* cfg = ShmupEnemyRegistry::get(enemy.typeId);
          if (cfg != nullptr) {
            processEnemyDrop(w, *cfg, enemyT.pos.x, enemyT.pos.y);
          }
          enemiesToRemove.push_back(enemyEnt);
        }

        if (proj.pierceRemaining <= 0) {
          projToRemove.push_back(projEnt);
          break;  // Projectile is done
        }
      }
    }
  }

  // Player projectiles vs bosses (with weak points)
  auto bossView = w.registry.view<ShmupBossTag, BossState, Transform, AABB>();
  for (auto [projEnt, proj, projT, projAABB] : projView.each()) {
    if (!proj.fromPlayer)
      continue;
    if (proj.pierceRemaining <= 0)
      continue;

    for (auto [bossEnt, boss, bossT, bossAABB] : bossView.each()) {
      // Boss invulnerable during entrance or death
      if (boss.entering || boss.dying)
        continue;

      bool hitSomething = false;
      float damageMultiplier = 1.0F;

      // Check weak points first (higher priority)
      if (!boss.phases.empty() &&
          static_cast<std::size_t>(boss.currentPhase) < boss.phases.size()) {
        const auto& phase = boss.phases[static_cast<std::size_t>(boss.currentPhase)];

        for (auto& wp : boss.weakPoints) {
          if (wp.destroyed)
            continue;

          // Check if weak point is active in current phase
          bool isActive = std::find(phase.weakPoints.begin(), phase.weakPoints.end(), wp.id) !=
                          phase.weakPoints.end();
          if (!isActive)
            continue;

          // AABB check at weak point offset
          float wpX = bossT.pos.x + wp.offsetX;
          float wpY = bossT.pos.y + wp.offsetY;
          float dx = std::abs(projT.pos.x - wpX);
          float dy = std::abs(projT.pos.y - wpY);
          float overlapX = (projAABB.w + wp.hitboxW) * 0.5F;
          float overlapY = (projAABB.h + wp.hitboxH) * 0.5F;

          if (dx < overlapX && dy < overlapY) {
            damageMultiplier = wp.damageMultiplier;
            hitSomething = true;

            // Weak point has its own health
            if (wp.health > 0) {
              wp.health -= static_cast<int>(proj.damage * damageMultiplier);
              if (wp.health <= 0) {
                wp.destroyed = true;
              }
            }
            break;  // Only hit one weak point
          }
        }
      }

      // If no weak point hit, check main body
      if (!hitSomething) {
        float dx = std::abs(projT.pos.x - bossT.pos.x);
        float dy = std::abs(projT.pos.y - bossT.pos.y);
        float overlapX = (projAABB.w + bossAABB.w) * 0.5F;
        float overlapY = (projAABB.h + bossAABB.h) * 0.5F;

        if (dx < overlapX && dy < overlapY) {
          hitSomething = true;
        }
      }

      if (hitSomething) {
        boss.currentHealth -= static_cast<int>(proj.damage * damageMultiplier);
        --proj.pierceRemaining;

        // Spawn hit spark at impact point
        spawnHitSpark(w, projT.pos.x, projT.pos.y, 0.0F);

        if (boss.currentHealth <= 0) {
          boss.dying = true;
          boss.currentHealth = 0;
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
    if (proj.fromPlayer)
      continue;

    for (auto [playerEnt, ship, playerT, playerAABB] : playerView.each()) {
      if (ship.invincibleFrames > 0)
        continue;

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

  // Expire old items
  std::vector<EntityId> itemsToRemove;
  auto itemView = w.registry.view<ShmupItemTag, ItemState>();
  for (auto [entity, state] : itemView.each()) {
    if (state.ageFrames >= state.lifetimeFrames) {
      itemsToRemove.push_back(entity);
    }
  }
  for (EntityId e : itemsToRemove) {
    if (w.registry.valid(e)) {
      w.destroy(e);
    }
  }
}

void ShmupSystems::enemyMovement(World& w, TimeStep ts) {
  auto view = w.registry.view<ShmupEnemyTag, Transform, Movement>();

  for (auto [entity, t, move] : view.each()) {
    switch (move.type) {
      case MovementType::None:
        // Stationary - do nothing
        break;

      case MovementType::Linear:
        t.pos.x += move.velocityX;
        t.pos.y += move.velocityY;
        break;

      case MovementType::Sine:
        move.phase += move.frequency;
        if (move.oscillateY) {
          t.pos.x += move.velocityX;
          t.pos.y += std::sin(move.phase) * move.amplitude * move.frequency;
        } else {
          t.pos.x += std::cos(move.phase) * move.amplitude * move.frequency;
          t.pos.y += move.velocityY;
        }
        break;

      case MovementType::Chase:
        if (w.player != kInvalidEntity && w.registry.valid(w.player)) {
          const auto& playerT = w.registry.get<Transform>(w.player);
          float dx = playerT.pos.x - t.pos.x;
          float dy = playerT.pos.y - t.pos.y;
          float targetAngle = std::atan2(dy, dx);

          // Smooth turn toward target
          float diff = targetAngle - move.currentAngle;
          while (diff > kPi)
            diff -= 2.0F * kPi;
          while (diff < -kPi)
            diff += 2.0F * kPi;

          diff = std::clamp(diff, -move.turnRate, move.turnRate);
          move.currentAngle += diff;

          t.pos.x += std::cos(move.currentAngle) * move.chaseSpeed;
          t.pos.y += std::sin(move.currentAngle) * move.chaseSpeed;
        }
        break;

      case MovementType::Orbit:
        move.orbitAngle += move.orbitSpeed;
        if (move.orbitCenter != kInvalidEntity && w.registry.valid(move.orbitCenter)) {
          const auto& centerT = w.registry.get<Transform>(move.orbitCenter);
          t.pos.x = centerT.pos.x + std::cos(move.orbitAngle) * move.orbitRadius;
          t.pos.y = centerT.pos.y + std::sin(move.orbitAngle) * move.orbitRadius;
        }
        break;

      case MovementType::Formation:
        if (move.formationLeader != kInvalidEntity && w.registry.valid(move.formationLeader)) {
          const auto& leaderT = w.registry.get<Transform>(move.formationLeader);
          t.pos.x = leaderT.pos.x + move.formationOffsetX;
          t.pos.y = leaderT.pos.y + move.formationOffsetY;
        }
        break;

      case MovementType::Compound:
        // Reserved for multi-pattern behavior
        break;
    }

    // Clamp to vertical bounds
    t.pos.y = std::clamp(t.pos.y, move.boundsMinY, move.boundsMaxY);
  }

  (void)ts;  // Reserved for time-based patterns
}

void ShmupSystems::enemyFiring(World& w, TimeStep ts) {
  auto view = w.registry.view<ShmupEnemyTag, Transform, Firing>();

  for (auto [entity, t, fire] : view.each()) {
    if (fire.type == FireType::None) {
      continue;
    }

    // Handle initial delay
    if (fire.initialDelay > 0.0F) {
      fire.initialDelay -= ts.dt;
      continue;
    }

    // Update fire timer
    fire.timeSinceFire += ts.dt;

    // Handle burst firing
    if (fire.shotsRemaining > 0) {
      fire.burstTimer -= ts.dt;
      if (fire.burstTimer <= 0.0F) {
        fireEnemyShot(w, entity, t, fire);
        --fire.shotsRemaining;
        fire.burstTimer = fire.burstInterval;
      }
      continue;
    }

    // Check if ready to fire
    if (fire.timeSinceFire >= fire.fireInterval) {
      fire.timeSinceFire = 0.0F;
      fire.shotsRemaining = fire.shotsPerBurst - 1;
      fire.burstTimer = fire.burstInterval;
      fireEnemyShot(w, entity, t, fire);
    }
  }
}

EntityId ShmupSystems::spawnEnemy(World& w, const ShmupEnemyConfig& cfg, float x, float y) {
  EntityId id = w.create();
  auto& reg = w.registry;

  reg.emplace<Transform>(id, Vec2{x, y});
  reg.emplace<AABB>(id, cfg.collision.w, cfg.collision.h);
  reg.emplace<ShmupEnemyTag>(id);

  ShmupEnemyState enemy;
  enemy.typeId = cfg.id;
  enemy.health = cfg.stats.health;
  enemy.maxHealth = cfg.stats.health;
  enemy.scoreValue = cfg.stats.scoreValue;
  enemy.damageOnContact = static_cast<int>(cfg.stats.contactDamage);
  enemy.invulnerable = cfg.stats.invulnerable;
  reg.emplace<ShmupEnemyState>(id, enemy);

  // Set up movement component
  Movement move;
  switch (cfg.movement.type) {
    case ShmupEnemyConfig::MovementType::None:
      move.type = MovementType::None;
      break;
    case ShmupEnemyConfig::MovementType::Linear:
      move.type = MovementType::Linear;
      break;
    case ShmupEnemyConfig::MovementType::Sine:
      move.type = MovementType::Sine;
      break;
    case ShmupEnemyConfig::MovementType::Chase:
      move.type = MovementType::Chase;
      break;
    case ShmupEnemyConfig::MovementType::Orbit:
      move.type = MovementType::Orbit;
      break;
    case ShmupEnemyConfig::MovementType::Formation:
      move.type = MovementType::Formation;
      break;
  }
  move.velocityX = cfg.movement.velocityX;
  move.velocityY = cfg.movement.velocityY;
  move.amplitude = cfg.movement.amplitude;
  move.frequency = cfg.movement.frequency;
  move.oscillateY = cfg.movement.oscillateY;
  move.chaseSpeed = cfg.movement.chaseSpeed;
  move.turnRate = cfg.movement.turnRate;
  move.orbitRadius = cfg.movement.orbitRadius;
  move.orbitSpeed = cfg.movement.orbitSpeed;
  reg.emplace<Movement>(id, move);

  // Set up firing component
  if (cfg.fire.type != ShmupEnemyConfig::FireType::None) {
    Firing fire;
    switch (cfg.fire.type) {
      case ShmupEnemyConfig::FireType::None:
        fire.type = FireType::None;
        break;
      case ShmupEnemyConfig::FireType::Aimed:
        fire.type = FireType::Aimed;
        break;
      case ShmupEnemyConfig::FireType::Spread:
        fire.type = FireType::Spread;
        break;
      case ShmupEnemyConfig::FireType::Circular:
        fire.type = FireType::Circular;
        break;
    }
    fire.fireInterval = cfg.fire.fireInterval;
    fire.initialDelay = static_cast<float>(cfg.fire.warmupFrames) / 60.0F;
    fire.shotCount = cfg.fire.shotCount;
    fire.spreadAngle = cfg.fire.spreadAngle;
    fire.rotationSpeed = cfg.fire.rotationSpeed;
    reg.emplace<Firing>(id, fire);
  }

  return id;
}

EntityId ShmupSystems::spawnBoss(World& w,
                                 const ShmupBossConfig& cfg,
                                 [[maybe_unused]] float x,
                                 [[maybe_unused]] float y) {
  EntityId id = w.create();
  auto& reg = w.registry;

  // Spawn at entrance start position (from config), will animate to target
  reg.emplace<Transform>(id, Vec2{cfg.entrance.startX, cfg.entrance.startY});
  reg.emplace<AABB>(id, cfg.collision.w, cfg.collision.h);
  reg.emplace<ShmupBossTag>(id);

  // Initialize boss state
  BossState boss;
  boss.bossId = cfg.id;
  boss.totalHealth = cfg.stats.totalHealth;
  boss.currentHealth = cfg.stats.totalHealth;
  boss.entering = true;
  boss.entranceProgress = 0.0F;
  boss.entranceDuration = cfg.entrance.durationSeconds;
  boss.targetX = cfg.entrance.targetX;
  boss.targetY = cfg.entrance.targetY;
  boss.currentPhase = 0;
  boss.dying = false;
  boss.explosionsRemaining = cfg.death.explosionCount;
  boss.explosionTimer = 0.0F;
  boss.minionTimer = 0.0F;

  // Copy phases from config
  for (const auto& phaseCfg : cfg.phases) {
    BossPhase phase;
    phase.id = phaseCfg.id;
    phase.healthThreshold = phaseCfg.healthThreshold;
    phase.weakPoints = phaseCfg.activeWeakPoints;
    phase.spawnMinions = phaseCfg.minions.enabled;
    phase.minionType = phaseCfg.minions.minionType;
    phase.minionInterval = phaseCfg.minions.spawnInterval;
    phase.speedMultiplier = phaseCfg.movement.speedMultiplier;
    boss.phases.push_back(phase);
  }

  // Copy weak points from config
  for (const auto& wpCfg : cfg.weakPoints) {
    WeakPoint wp;
    wp.id = wpCfg.id;
    wp.offsetX = wpCfg.offsetX;
    wp.offsetY = wpCfg.offsetY;
    wp.hitboxW = wpCfg.hitboxW;
    wp.hitboxH = wpCfg.hitboxH;
    wp.damageMultiplier = wpCfg.damageMultiplier;
    wp.health = wpCfg.health;
    wp.destroyed = false;
    boss.weakPoints.push_back(wp);
  }

  reg.emplace<BossState>(id, boss);

  // Set up initial movement from phase 0
  Movement move;
  if (!cfg.phases.empty()) {
    const auto& phase0 = cfg.phases[0];
    move.type = mapBossMovementType(phase0.movement.type);
    move.velocityX = phase0.movement.velocityX;
    move.velocityY = phase0.movement.velocityY;
    move.amplitude = phase0.movement.amplitude;
    move.frequency = phase0.movement.frequency;
    move.oscillateY = phase0.movement.oscillateY;
    move.chaseSpeed = phase0.movement.chaseSpeed;
    move.turnRate = phase0.movement.turnRate;
    move.boundsMinY = phase0.movement.boundsMinY;
    move.boundsMaxY = phase0.movement.boundsMaxY;
  }
  reg.emplace<Movement>(id, move);

  // Set up initial firing from phase 0
  Firing fire;
  if (!cfg.phases.empty()) {
    const auto& phase0 = cfg.phases[0];
    fire.type = mapBossFireType(phase0.fire.type);
    fire.fireInterval = phase0.fire.fireInterval;
    fire.shotCount = phase0.fire.shotCount;
    fire.spreadAngle = phase0.fire.spreadAngle;
    fire.rotationSpeed = phase0.fire.rotationSpeed;
    fire.shotsPerBurst = phase0.fire.burstCount;
    fire.burstInterval = phase0.fire.burstInterval;
  }
  reg.emplace<Firing>(id, fire);

  return id;
}

void ShmupSystems::boss(World& w, TimeStep ts) {
  auto view = w.registry.view<ShmupBossTag, BossState, Transform, Movement, Firing>();

  std::vector<EntityId> toRemove;

  for (auto [entity, boss, t, move, fire] : view.each()) {
    // 1. Entrance animation (lerp from start to target)
    if (boss.entering) {
      boss.entranceProgress += ts.dt;
      float ratio = std::min(boss.entranceProgress / boss.entranceDuration, 1.0F);
      float eased = easeOutCubic(ratio);

      // Calculate start position from target + direction offset
      // Boss enters from right side of screen
      constexpr float kEntranceOffsetX = 400.0F;
      float startX = boss.targetX + kEntranceOffsetX;
      float startY = boss.targetY;

      t.pos.x = lerp(startX, boss.targetX, eased);
      t.pos.y = lerp(startY, boss.targetY, eased);

      if (ratio >= 1.0F) {
        boss.entering = false;
        // Snap to exact target position
        t.pos.x = boss.targetX;
        t.pos.y = boss.targetY;
      }
      continue;  // Skip other logic during entrance
    }

    // 2. Death sequence (spawn explosions, then despawn)
    if (boss.dying) {
      constexpr float kExplosionInterval = 0.15F;
      boss.explosionTimer += ts.dt;
      if (boss.explosionTimer >= kExplosionInterval && boss.explosionsRemaining > 0) {
        // Spawn explosion at pseudo-random offset from boss center
        // Use explosionsRemaining as seed for variety
        float seed = static_cast<float>(boss.explosionsRemaining) * 2.3999F;
        float offsetX = std::sin(seed) * 60.0F;
        float offsetY = std::cos(seed * 1.618F) * 40.0F;
        spawnExplosion(w, t.pos.x + offsetX, t.pos.y + offsetY, 1.5F);
        --boss.explosionsRemaining;
        boss.explosionTimer = 0.0F;
      }

      if (boss.explosionsRemaining <= 0) {
        // Final big explosion
        spawnExplosion(w, t.pos.x, t.pos.y, 2.5F);

        // Add boss score before removal
        const ShmupBossConfig* cfg = ShmupBossRegistry::get(boss.bossId);
        if (cfg != nullptr) {
          w.score += cfg->stats.scoreValue;
        }
        toRemove.push_back(entity);
      }
      continue;  // Skip other logic during death
    }

    // 3. Phase transitions (when health crosses threshold)
    float healthRatio =
        static_cast<float>(boss.currentHealth) / static_cast<float>(boss.totalHealth);

    // Find the appropriate phase based on current health
    for (std::size_t i = static_cast<std::size_t>(boss.currentPhase) + 1; i < boss.phases.size();
         ++i) {
      if (healthRatio <= boss.phases[i].healthThreshold) {
        boss.currentPhase = static_cast<int>(i);

        // Update movement and firing for new phase
        // This requires access to the ShmupBossConfig, but we don't have it here.
        // Instead, we store pattern IDs in phases and would need a different approach.
        // For MVP, phases just affect minion spawning and speed multiplier.
        break;
      }
    }

    // 4. Minion spawning
    if (!boss.phases.empty() && static_cast<std::size_t>(boss.currentPhase) < boss.phases.size()) {
      const auto& phase = boss.phases[static_cast<std::size_t>(boss.currentPhase)];
      if (phase.spawnMinions) {
        boss.minionTimer += ts.dt;
        if (boss.minionTimer >= phase.minionInterval) {
          boss.minionTimer = 0.0F;
          // Spawn minion at offset from boss position
          const ShmupEnemyConfig* minionCfg = ShmupEnemyRegistry::get(phase.minionType);
          if (minionCfg != nullptr) {
            ShmupSystems::spawnEnemy(w, *minionCfg, t.pos.x - 80.0F, t.pos.y);
          }
        }
      }
    }

    // Boss movement is handled by enemyMovement() system since it uses the same Movement component
    // Boss firing is handled by enemyFiring() system since it uses the same Firing component
  }

  for (EntityId e : toRemove) {
    if (w.registry.valid(e)) {
      w.destroy(e);
    }
  }
}

// WaveSpawner implementation
void WaveSpawner::init(const LevelConfig* levelCfg) {
  level = levelCfg;
  nextWaveIndex = 0;
  nextBossIndex = 0;
  lastScrollX = 0.0F;
}

void WaveSpawner::update(World& w, float scrollX) {
  if (level == nullptr) {
    return;
  }

  // Check for waves to spawn
  while (nextWaveIndex < level->waves.size()) {
    const auto& wave = level->waves[nextWaveIndex];

    // Check if we've scrolled past the trigger position
    if (scrollX < wave.triggerPosition) {
      break;  // Not yet triggered
    }

    // Spawn this wave
    const ShmupEnemyConfig* enemyCfg = ShmupEnemyRegistry::get(wave.enemyType);
    if (enemyCfg == nullptr) {
      ++nextWaveIndex;
      continue;
    }

    // Calculate spawn positions based on formation
    std::vector<Vec2> positions;
    float baseX = wave.spawnX;
    float baseY = wave.spawnY;

    switch (wave.formation) {
      case LevelConfig::Formation::Single:
        positions.push_back({baseX, baseY});
        break;

      case LevelConfig::Formation::Line:
        for (int i = 0; i < wave.count; ++i) {
          positions.push_back(
              {baseX, baseY + (static_cast<float>(i) - static_cast<float>(wave.count - 1) * 0.5F) *
                                  wave.spacing});
        }
        break;

      case LevelConfig::Formation::Column:
        for (int i = 0; i < wave.count; ++i) {
          positions.push_back({baseX + static_cast<float>(i) * wave.spacing, baseY});
        }
        break;

      case LevelConfig::Formation::VShape:
        for (int i = 0; i < wave.count; ++i) {
          float offset = static_cast<float>(i) - static_cast<float>(wave.count - 1) * 0.5F;
          positions.push_back(
              {baseX + std::abs(offset) * wave.spacing * 0.5F, baseY + offset * wave.spacing});
        }
        break;

      case LevelConfig::Formation::InverseV:
        for (int i = 0; i < wave.count; ++i) {
          float offset = static_cast<float>(i) - static_cast<float>(wave.count - 1) * 0.5F;
          positions.push_back(
              {baseX - std::abs(offset) * wave.spacing * 0.5F, baseY + offset * wave.spacing});
        }
        break;

      case LevelConfig::Formation::Diagonal:
        for (int i = 0; i < wave.count; ++i) {
          positions.push_back({baseX + static_cast<float>(i) * wave.spacing * 0.5F,
                               baseY + static_cast<float>(i) * wave.spacing});
        }
        break;

      case LevelConfig::Formation::Random:
        for (int i = 0; i < wave.count; ++i) {
          float rx = baseX + static_cast<float>((i * 7 + 13) % 100) * 0.01F * wave.spacing;
          float ry = baseY + static_cast<float>((i * 17 + 7) % 100 - 50) * 0.02F * wave.spacing;
          positions.push_back({rx, ry});
        }
        break;
    }

    // Spawn enemies at calculated positions
    for (const auto& pos : positions) {
      ShmupSystems::spawnEnemy(w, *enemyCfg, pos.x, pos.y);
    }

    ++nextWaveIndex;
  }

  // Check for boss sections to spawn
  while (nextBossIndex < level->bossSections.size()) {
    const auto& bossSection = level->bossSections[nextBossIndex];

    // Check if we've scrolled past the trigger position
    if (scrollX < bossSection.triggerPosition) {
      break;  // Not yet triggered
    }

    // Spawn the boss
    const ShmupBossConfig* bossCfg = ShmupBossRegistry::get(bossSection.bossId);
    if (bossCfg != nullptr) {
      ShmupSystems::spawnBoss(w, *bossCfg, bossCfg->entrance.startX, bossCfg->entrance.startY);
    }

    ++nextBossIndex;
  }

  lastScrollX = scrollX;
}

void WaveSpawner::reset() {
  level = nullptr;
  nextWaveIndex = 0;
  nextBossIndex = 0;
  lastScrollX = 0.0F;
}

// =============================================================================
// Item Systems
// =============================================================================

EntityId ShmupSystems::spawnItem(World& w, const ShmupItemConfig& cfg, float x, float y) {
  EntityId id = w.create();
  auto& reg = w.registry;

  reg.emplace<Transform>(id, Vec2{x, y});
  reg.emplace<ShmupItemTag>(id);

  // Set up item state
  ItemState state;
  state.itemId = cfg.id;
  // Map config effect type to component effect type
  switch (cfg.effect.type) {
    case ShmupItemConfig::EffectType::WeaponUpgrade:
      state.effectType = ItemEffectType::WeaponUpgrade;
      break;
    case ShmupItemConfig::EffectType::Life:
      state.effectType = ItemEffectType::Life;
      break;
    case ShmupItemConfig::EffectType::Bomb:
      state.effectType = ItemEffectType::Bomb;
      break;
    case ShmupItemConfig::EffectType::ScoreBonus:
      state.effectType = ItemEffectType::ScoreBonus;
      break;
    case ShmupItemConfig::EffectType::Shield:
      state.effectType = ItemEffectType::Shield;
      break;
    case ShmupItemConfig::EffectType::FullPower:
      state.effectType = ItemEffectType::FullPower;
      break;
  }
  state.effectValue = cfg.effect.value;
  state.lifetimeFrames = cfg.pickup.lifetimeFrames;
  state.pickupRadius = cfg.pickup.radius;
  state.ageFrames = 0;
  state.magnetized = false;
  reg.emplace<ItemState>(id, state);

  // Set up item movement
  ItemMovement move;
  switch (cfg.movement.type) {
    case ShmupItemConfig::MovementType::Float:
      move.type = ItemMovementType::Float;
      break;
    case ShmupItemConfig::MovementType::Bounce:
      move.type = ItemMovementType::Bounce;
      break;
    case ShmupItemConfig::MovementType::Magnet:
      move.type = ItemMovementType::Magnet;
      break;
    case ShmupItemConfig::MovementType::Stationary:
      move.type = ItemMovementType::Stationary;
      break;
  }
  move.velocityX = cfg.movement.velocityX;
  move.velocityY = cfg.movement.velocityY;
  move.magnetRange = cfg.pickup.magnetRange;
  move.magnetSpeed = cfg.pickup.magnetSpeed;
  move.bounceSpeed = cfg.movement.bounceSpeed;
  reg.emplace<ItemMovement>(id, move);

  return id;
}

void ShmupSystems::processEnemyDrop(World& w, const ShmupEnemyConfig& cfg, float x, float y) {
  if (cfg.drops.entries.empty()) {
    return;
  }

  bool droppedSomething = false;

  for (const auto& entry : cfg.drops.entries) {
    float roll = randomFloat();
    if (roll < entry.probability) {
      const ShmupItemConfig* itemCfg = ShmupItemRegistry::get(entry.itemId);
      if (itemCfg != nullptr) {
        spawnItem(w, *itemCfg, x, y);
        droppedSomething = true;
      }
    }
  }

  // Handle guaranteed drop - if nothing dropped, force the first entry
  if (!droppedSomething && cfg.drops.guaranteedDrop && !cfg.drops.entries.empty()) {
    const auto& entry = cfg.drops.entries[0];
    const ShmupItemConfig* itemCfg = ShmupItemRegistry::get(entry.itemId);
    if (itemCfg != nullptr) {
      spawnItem(w, *itemCfg, x, y);
    }
  }
}

void ShmupSystems::itemMovement(World& w, TimeStep ts) {
  auto view = w.registry.view<ShmupItemTag, ItemState, ItemMovement, Transform>();

  constexpr float kScreenW = 1280.0F;
  constexpr float kScreenH = 720.0F;
  constexpr float kMargin = 16.0F;

  for (auto [entity, state, move, t] : view.each()) {
    // Check for magnetization (player proximity)
    if (w.player != kInvalidEntity && w.registry.valid(w.player)) {
      const auto& playerT = w.registry.get<Transform>(w.player);
      float dx = playerT.pos.x - t.pos.x;
      float dy = playerT.pos.y - t.pos.y;
      float distSq = dx * dx + dy * dy;

      if (distSq < move.magnetRange * move.magnetRange) {
        state.magnetized = true;
      }
    }

    if (state.magnetized && w.player != kInvalidEntity && w.registry.valid(w.player)) {
      // Move toward player
      const auto& playerT = w.registry.get<Transform>(w.player);
      float dx = playerT.pos.x - t.pos.x;
      float dy = playerT.pos.y - t.pos.y;
      float dist = std::sqrt(dx * dx + dy * dy);
      if (dist > 1.0F) {
        t.pos.x += (dx / dist) * move.magnetSpeed;
        t.pos.y += (dy / dist) * move.magnetSpeed;
      }
    } else {
      // Normal movement based on type
      switch (move.type) {
        case ItemMovementType::Float:
          t.pos.x += move.velocityX;
          t.pos.y += move.velocityY;
          break;

        case ItemMovementType::Bounce:
          t.pos.x += move.velocityX;
          t.pos.y += move.velocityY;
          // Bounce off screen edges
          if (t.pos.x < kMargin) {
            t.pos.x = kMargin;
            move.velocityX = std::abs(move.velocityX);
          } else if (t.pos.x > kScreenW - kMargin) {
            t.pos.x = kScreenW - kMargin;
            move.velocityX = -std::abs(move.velocityX);
          }
          if (t.pos.y < kMargin) {
            t.pos.y = kMargin;
            move.velocityY = std::abs(move.velocityY);
          } else if (t.pos.y > kScreenH - kMargin) {
            t.pos.y = kScreenH - kMargin;
            move.velocityY = -std::abs(move.velocityY);
          }
          break;

        case ItemMovementType::Magnet:
          // Always magnetize (handled above when player exists)
          break;

        case ItemMovementType::Stationary:
          // No movement
          break;
      }
    }

    // Increment age
    ++state.ageFrames;
  }

  (void)ts;  // Reserved for time-based animations
}

void ShmupSystems::itemPickup(World& w) {
  auto itemView = w.registry.view<ShmupItemTag, ItemState, Transform>();
  auto playerView = w.registry.view<ShmupPlayerTag, ShipState, WeaponState, Transform>();

  std::vector<EntityId> toRemove;

  for (auto [itemEnt, itemState, itemT] : itemView.each()) {
    for (auto [playerEnt, ship, weapons, playerT] : playerView.each()) {
      float dx = playerT.pos.x - itemT.pos.x;
      float dy = playerT.pos.y - itemT.pos.y;
      float distSq = dx * dx + dy * dy;
      float pickupRadiusSq = itemState.pickupRadius * itemState.pickupRadius;

      if (distSq < pickupRadiusSq) {
        // Apply effect based on type
        switch (itemState.effectType) {
          case ItemEffectType::WeaponUpgrade:
            // Upgrade active weapon level (max 4)
            if (!weapons.mounts.empty()) {
              auto& mount = weapons.mounts[static_cast<std::size_t>(weapons.activeSlot)];
              mount.level = std::min(4, mount.level + itemState.effectValue);
            }
            break;

          case ItemEffectType::Life:
            ship.lives += itemState.effectValue;
            break;

          case ItemEffectType::Bomb:
            // Would need SessionState for bombs - skip for now
            break;

          case ItemEffectType::ScoreBonus:
            w.score += itemState.effectValue;
            break;

          case ItemEffectType::Shield:
            ship.invincibleFrames = std::max(ship.invincibleFrames, itemState.effectValue);
            break;

          case ItemEffectType::FullPower:
            for (auto& mount : weapons.mounts) {
              mount.level = 4;  // MAX
            }
            break;
        }

        toRemove.push_back(itemEnt);
        break;  // Item collected
      }
    }
  }

  for (EntityId e : toRemove) {
    if (w.registry.valid(e)) {
      w.destroy(e);
    }
  }
}

void ShmupSystems::playerDeath(World& w) {
  auto view = w.registry.view<ShmupPlayerTag, ShipState, Transform>();

  for (auto [entity, ship, t] : view.each()) {
    if (ship.health <= 0) {
      if (ship.lives > 0) {
        // Respawn with one less life
        --ship.lives;
        ship.health = 3;
        ship.invincibleFrames = 180;   // 3 seconds at 60fps
        t.pos = Vec2{200.0F, 360.0F};  // Respawn at left-center

        // Downgrade weapon on death (classic SHMUP penalty)
        if (auto* weapons = w.registry.try_get<WeaponState>(entity)) {
          for (auto& mount : weapons->mounts) {
            mount.level = std::max(1, mount.level - 1);
          }
        }
      }
      // If lives == 0, game over handled by ShmupGame
    }
  }
}

// =============================================================================
// Visual Effects
// =============================================================================

void ShmupSystems::spawnExplosion(World& w, float x, float y, float scale) {
  EntityId id = w.create();
  w.registry.emplace<Transform>(id, Vec2{x, y});
  w.registry.emplace<ShmupEffectTag>(id);

  EffectState effect;
  effect.type = EffectType::Explosion;
  effect.lifetimeFrames = 24;  // ~0.4 seconds
  effect.scale = scale;
  w.registry.emplace<EffectState>(id, effect);
}

void ShmupSystems::spawnHitSpark(World& w, float x, float y, float angle) {
  EntityId id = w.create();
  w.registry.emplace<Transform>(id, Vec2{x, y});
  w.registry.emplace<ShmupEffectTag>(id);

  EffectState effect;
  effect.type = EffectType::HitSpark;
  effect.lifetimeFrames = 8;  // Quick flash
  effect.rotation = angle;
  effect.scale = 0.5F;
  w.registry.emplace<EffectState>(id, effect);
}

void ShmupSystems::effects(World& w, [[maybe_unused]] TimeStep ts) {
  auto view = w.registry.view<ShmupEffectTag, EffectState>();
  std::vector<EntityId> toRemove;

  for (auto [entity, effect] : view.each()) {
    ++effect.ageFrames;
    if (effect.ageFrames >= effect.lifetimeFrames) {
      toRemove.push_back(entity);
    }
  }

  for (EntityId e : toRemove) {
    if (w.registry.valid(e)) {
      w.destroy(e);
    }
  }
}

}  // namespace shmup
