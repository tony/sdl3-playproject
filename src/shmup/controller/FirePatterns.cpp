#include "shmup/controller/FirePatterns.h"

#include <cmath>

#include "ecs/World.h"
#include "shmup/components/ShmupComponents.h"
#include "shmup/config/WeaponConfig.h"

namespace shmup {

namespace {

constexpr float kPi = 3.14159265358979323846F;

EntityId spawnProjectile(World& w,
                         const WeaponConfig& cfg,
                         const WeaponConfig::LevelStats& stats,
                         Vec2 pos,
                         float angle,
                         bool fromPlayer) {
  EntityId id = w.create();
  auto& reg = w.registry;

  reg.emplace<Transform>(id, pos);

  const float speed = stats.projectileSpeed;
  Velocity vel;
  vel.v.x = std::cos(angle) * speed;
  vel.v.y = std::sin(angle) * speed;
  reg.emplace<Velocity>(id, vel);

  reg.emplace<AABB>(id, cfg.projectile.w, cfg.projectile.h);
  reg.emplace<ShmupProjectileTag>(id);

  ShmupProjectileState proj;
  proj.damage = stats.damage;
  proj.lifetimeFrames = cfg.projectile.lifetimeFrames;
  proj.pierceRemaining = stats.pierce;
  proj.fromPlayer = fromPlayer;
  proj.gravity = cfg.projectile.gravity;
  reg.emplace<ShmupProjectileState>(id, proj);

  return id;
}

}  // namespace

void FirePatterns::fire(World& w,
                        const WeaponConfig& cfg,
                        const WeaponConfig::LevelStats& stats,
                        Vec2 firePos,
                        float baseAngle,
                        bool fromPlayer) {
  switch (cfg.pattern) {
    case WeaponConfig::PatternType::Single:
      fireSingle(w, cfg, stats, firePos, baseAngle, fromPlayer);
      break;
    case WeaponConfig::PatternType::Spread:
      fireSpread(w, cfg, stats, firePos, baseAngle, fromPlayer);
      break;
    case WeaponConfig::PatternType::Homing:
      fireHoming(w, cfg, stats, firePos, baseAngle, fromPlayer);
      break;
    case WeaponConfig::PatternType::Wave:
      fireWave(w, cfg, stats, firePos, baseAngle, fromPlayer);
      break;
    case WeaponConfig::PatternType::Laser:
      // Laser is handled differently (continuous beam, not projectiles)
      // For now, treat as single shot
      fireSingle(w, cfg, stats, firePos, baseAngle, fromPlayer);
      break;
  }
}

void FirePatterns::fireSingle(World& w,
                              const WeaponConfig& cfg,
                              const WeaponConfig::LevelStats& stats,
                              Vec2 firePos,
                              float baseAngle,
                              bool fromPlayer) {
  // Single fires multiple projectiles in a narrow spread (based on level)
  const int count = stats.projectileCount;
  const float spread = stats.spreadAngle;

  if (count == 1) {
    // Single projectile straight ahead
    spawnProjectile(w, cfg, stats, firePos, baseAngle, fromPlayer);
  } else {
    // Multiple projectiles in a fan
    const float step = spread / static_cast<float>(count - 1);
    float startAngle = baseAngle - spread * 0.5F;

    for (int i = 0; i < count; ++i) {
      float angle = startAngle + step * static_cast<float>(i);
      spawnProjectile(w, cfg, stats, firePos, angle, fromPlayer);
    }
  }
}

void FirePatterns::fireSpread(World& w,
                              const WeaponConfig& cfg,
                              const WeaponConfig::LevelStats& stats,
                              Vec2 firePos,
                              float baseAngle,
                              bool fromPlayer) {
  // Spread is like single but always fans out
  const int count = stats.projectileCount;
  const float spread = stats.spreadAngle;

  if (count == 1) {
    spawnProjectile(w, cfg, stats, firePos, baseAngle, fromPlayer);
    return;
  }

  const float step = spread / static_cast<float>(count - 1);
  float startAngle = baseAngle - spread * 0.5F;

  for (int i = 0; i < count; ++i) {
    float angle = startAngle + step * static_cast<float>(i);
    spawnProjectile(w, cfg, stats, firePos, angle, fromPlayer);
  }
}

void FirePatterns::fireHoming(World& w,
                              const WeaponConfig& cfg,
                              const WeaponConfig::LevelStats& stats,
                              Vec2 firePos,
                              float baseAngle,
                              bool fromPlayer) {
  // Spawn homing projectiles
  const int count = stats.projectileCount;
  const float spread = stats.spreadAngle;

  if (count == 1) {
    EntityId id = spawnProjectile(w, cfg, stats, firePos, baseAngle, fromPlayer);
    // Add homing component
    HomingState homing;
    homing.turnRate = cfg.projectile.homingTurnRate;
    homing.seekRadius = cfg.projectile.homingSeekRadius;
    homing.delayFrames = cfg.projectile.homingDelayFrames;
    homing.target = kInvalidEntity;
    w.registry.emplace<HomingState>(id, homing);
  } else {
    const float step = spread / static_cast<float>(count - 1);
    float startAngle = baseAngle - spread * 0.5F;

    for (int i = 0; i < count; ++i) {
      float angle = startAngle + step * static_cast<float>(i);
      EntityId id = spawnProjectile(w, cfg, stats, firePos, angle, fromPlayer);

      HomingState homing;
      homing.turnRate = cfg.projectile.homingTurnRate;
      homing.seekRadius = cfg.projectile.homingSeekRadius;
      homing.delayFrames = cfg.projectile.homingDelayFrames;
      homing.target = kInvalidEntity;
      w.registry.emplace<HomingState>(id, homing);
    }
  }
}

void FirePatterns::fireWave(World& w,
                            const WeaponConfig& cfg,
                            const WeaponConfig::LevelStats& stats,
                            Vec2 firePos,
                            float baseAngle,
                            bool fromPlayer) {
  // Wave pattern: projectiles that oscillate vertically
  // The oscillation is handled by a special component, but for now
  // we just spawn with slight vertical velocity variation
  const int count = stats.projectileCount;

  for (int i = 0; i < count; ++i) {
    // Stagger spawn positions slightly
    Vec2 offset{static_cast<float>(i) * 8.0F, 0.0F};
    EntityId id = spawnProjectile(w, cfg, stats, firePos + offset, baseAngle, fromPlayer);

    // Wave motion handled by velocity variation for now
    (void)id;
  }
}

}  // namespace shmup
