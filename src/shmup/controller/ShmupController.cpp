#include "shmup/controller/ShmupController.h"

#include "core/Time.h"
#include "ecs/Components.h"
#include "ecs/World.h"
#include "shmup/components/ShmupComponents.h"
#include "shmup/config/LevelConfig.h"
#include "shmup/config/SatelliteConfig.h"
#include "shmup/config/ShipConfig.h"
#include "shmup/config/ShmupBossConfig.h"
#include "shmup/config/WeaponConfig.h"
#include "shmup/systems/ShmupSystems.h"

namespace shmup {

EntityId ShmupController::spawnPlayer(World& world,
                                      const ShipConfig& shipCfg,
                                      float startX,
                                      float startY) {
  EntityId id = world.create();
  auto& reg = world.registry;

  // Core components
  reg.emplace<Transform>(id, Vec2{startX, startY});
  reg.emplace<Velocity>(id);
  reg.emplace<AABB>(id, shipCfg.collision.w, shipCfg.collision.h);
  reg.emplace<InputState>(id);

  // SHMUP-specific components
  reg.emplace<ShmupPlayerTag>(id);

  ShipState ship;
  ship.shipId = shipCfg.id;
  ship.health = 3;
  ship.lives = lives_;
  ship.speed = shipCfg.physics.speed;
  ship.focusedSpeed = shipCfg.physics.focusedSpeed;
  ship.instantMovement = shipCfg.physics.instantMovement;
  reg.emplace<ShipState>(id, ship);

  // Set up weapon mounts from ship config
  WeaponState weapons;
  weapons.firing = false;
  for (const auto& slot : shipCfg.weaponSlots) {
    WeaponMount mount;
    mount.weaponId = slot.defaultWeapon;
    mount.level = slot.defaultLevel;
    mount.offsetX = slot.offsetX;
    mount.offsetY = slot.offsetY;
    mount.cooldownFrames = 0;
    weapons.mounts.push_back(mount);
  }
  reg.emplace<WeaponState>(id, weapons);

  // Add sprite from ship render config
  if (!shipCfg.render.sheets.empty()) {
    ShmupSprite sprite;
    // Use "east" direction (facing right) for horizontal shmup
    auto it = shipCfg.render.sheets.find("east");
    if (it != shipCfg.render.sheets.end()) {
      sprite.texturePath = it->second;
    }
    sprite.frameW = shipCfg.render.frameW;
    sprite.frameH = shipCfg.render.frameH;
    sprite.scale = shipCfg.render.scale;
    sprite.offsetX = shipCfg.render.offsetX;
    sprite.offsetY = shipCfg.render.offsetY;
    reg.emplace<ShmupSprite>(id, sprite);
  }

  player_ = id;
  world.player = id;  // Also set World's player handle

  // Spawn satellites from ship config
  for (std::size_t i = 0; i < shipCfg.satelliteSlots.size(); ++i) {
    const auto& slot = shipCfg.satelliteSlots[i];
    const SatelliteConfig* satCfg = SatelliteRegistry::get(slot.defaultSatellite);
    if (satCfg != nullptr) {
      spawnSatellite(world, id, *satCfg, static_cast<int>(i));
    }
  }

  return id;
}

EntityId ShmupController::spawnSatellite(World& world,
                                         EntityId owner,
                                         const SatelliteConfig& satCfg,
                                         int slotIndex) {
  EntityId id = world.create();
  auto& reg = world.registry;

  // Get owner transform for initial position
  const auto& ownerT = reg.get<Transform>(owner);

  reg.emplace<Transform>(id, ownerT.pos);
  reg.emplace<ShmupSatelliteTag>(id);

  SatelliteState sat;
  sat.configId = satCfg.id;
  sat.owner = owner;
  sat.slotIndex = slotIndex;
  sat.orbitAngle = 0.0F;

  // Convert SatelliteConfig::PositionMode to SatelliteState::PositionMode
  switch (satCfg.positionMode) {
    case SatelliteConfig::PositionMode::Fixed:
      sat.positionMode = SatelliteState::PositionMode::Fixed;
      break;
    case SatelliteConfig::PositionMode::Orbit:
      sat.positionMode = SatelliteState::PositionMode::Orbit;
      break;
    case SatelliteConfig::PositionMode::Formation:
      sat.positionMode = SatelliteState::PositionMode::Formation;
      break;
  }

  // Copy position config
  sat.fixedOffsetX = satCfg.position.fixedOffsetX;
  sat.fixedOffsetY = satCfg.position.fixedOffsetY;
  sat.orbitRadius = satCfg.position.orbitRadius;
  sat.orbitSpeed = satCfg.position.orbitSpeed;
  sat.focusedRadius = satCfg.position.focusedRadius;
  sat.focusedOffsetX = satCfg.position.focusedOffsetX;
  sat.focusedOffsetY = satCfg.position.focusedOffsetY;

  // Mirror position for odd-indexed slots (left/right symmetry)
  if (slotIndex % 2 == 1) {
    sat.fixedOffsetX = -sat.fixedOffsetX;
    sat.focusedOffsetX = -sat.focusedOffsetX;
  }

  reg.emplace<SatelliteState>(id, sat);

  // Add sprite from satellite render config
  if (!satCfg.render.sprite.empty()) {
    ShmupSprite sprite;
    sprite.texturePath = satCfg.render.sprite;
    sprite.frameW = satCfg.render.frameW;
    sprite.frameH = satCfg.render.frameH;
    sprite.frameCount = satCfg.render.frameCount;
    sprite.frameRate = satCfg.render.fps;
    sprite.scale = satCfg.render.scale;
    reg.emplace<ShmupSprite>(id, sprite);
  }

  satellites_.push_back(id);
  return id;
}

void ShmupController::loadLevel(const LevelConfig* levelCfg) {
  waveSpawner_.init(levelCfg);
  if (levelCfg != nullptr) {
    scrollSpeed_ = levelCfg->properties.scrollSpeed;
  }
}

void ShmupController::update(World& world, TimeStep ts) {
  if (paused_) {
    return;
  }

  // Check if a boss is currently active (pause scroll during boss fights)
  auto bossView = world.registry.view<ShmupBossTag, BossState>();
  bool bossActive = bossView.begin() != bossView.end();

  // Only scroll if no boss is active
  if (!bossActive) {
    scrollX_ += scrollSpeed_ * ts.dt;
  }

  // Spawn waves and bosses based on scroll position
  waveSpawner_.update(world, scrollX_);

  // Run SHMUP systems in order
  ShmupSystems::playerMovement(world, ts);
  ShmupSystems::satelliteUpdate(world);
  ShmupSystems::weaponFiring(world, ts);
  ShmupSystems::enemyMovement(world, ts);
  ShmupSystems::enemyFiring(world, ts);
  ShmupSystems::boss(world, ts);  // Boss state machine (entrance, phases, death)
  ShmupSystems::projectiles(world, ts);
  ShmupSystems::collision(world);
  ShmupSystems::itemMovement(world, ts);
  ShmupSystems::itemPickup(world);
  ShmupSystems::playerDeath(world);
  ShmupSystems::effects(world, ts);
  ShmupSystems::cleanup(world);
}

}  // namespace shmup
