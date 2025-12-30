#include "ecs/Systems.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "character/CharacterConfig.h"
#include "character/CharacterController.h"
#include "collectible/CollectibleConfig.h"
#include "core/Time.h"
#include "ecs/Components.h"
#include "ecs/Entity.h"
#include "ecs/World.h"
#include "enemy/EnemyConfig.h"
#include "stage/Stage.h"
#include "util/Math.h"

namespace Systems {

namespace {
// Convert milliseconds to frames at 120Hz fixed timestep.
constexpr int msToFrames(int ms) {
  return (ms * 120 + 500) / 1000;  // +500 for rounding
}
}  // namespace

static bool
aabbOverlap(float ax, float ay, float aw, float ah, float bx, float by, float bw, float bh) {
  return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

static void spawnEnemyProjectile(World& w,
                                 const EnemyConfig& cfg,
                                 const EnemyState& state,
                                 const Transform& t,
                                 const AABB& box) {
  const EnemyConfig::Projectile& proj = cfg.shooter.projectile;
  const float facing = static_cast<float>(state.facingX);
  const float baseX = t.pos.x + box.w * 0.5F;
  const float baseY = t.pos.y + box.h * 0.5F;
  const float spawnX = baseX + proj.offsetX * facing - proj.w * 0.5F;
  const float spawnY = baseY + proj.offsetY - proj.h * 0.5F;

  const EntityId e = w.create();
  w.registry.emplace<Transform>(e, Vec2{spawnX, spawnY});
  w.registry.emplace<Velocity>(e, Vec2{facing * proj.speed, 0.0F});
  w.registry.emplace<AABB>(e, proj.w, proj.h);
  w.registry.emplace<ProjectileTag>(e);

  ProjectileState pstate{};
  pstate.damage = proj.damage;
  pstate.gravity = proj.gravity;
  pstate.knockbackVx = proj.knockbackVx;
  pstate.knockbackVy = proj.knockbackVy;
  pstate.lifetimeFrames = proj.lifetimeFrames;
  pstate.iframesMs = cfg.combat.iframesMs;
  pstate.fromEnemy = true;
  w.registry.emplace<ProjectileState>(e, pstate);
}

void characterController(World& w, Stage& s, TimeStep ts) {
  CharacterController::tick(w, s, ts);
}

void enemies(World& w, Stage& s, TimeStep ts) {
  const int dtms = static_cast<int>(std::round(ts.dt * 1000.0F));

  auto view =
      w.registry.view<EnemyTag, Transform, Velocity, AABB, Grounded, EnemyState, EnemyConfig>();
  for (auto entity : view) {
    auto& t = view.get<Transform>(entity);
    auto& v = view.get<Velocity>(entity);
    const auto& box = view.get<AABB>(entity);
    auto& g = view.get<Grounded>(entity);
    auto& state = view.get<EnemyState>(entity);
    const auto& cfg = view.get<EnemyConfig>(entity);

    if (state.invincibleMs > 0) {
      state.invincibleMs = std::max(0, state.invincibleMs - dtms);
    }

    if (state.aiTimerFrames > 0) {
      --state.aiTimerFrames;
    }
    if (state.attackTimerFrames > 0) {
      --state.attackTimerFrames;
    }

    const Rect aabb{t.pos.x, t.pos.y, box.w, box.h};
    if (cfg.move.turnOnWall && s.touchingWall(aabb, state.facingX, 1.0F)) {
      state.facingX *= -1;
    }
    if (cfg.move.turnOnEdge && g.onGround) {
      const float edgeProbeX = (state.facingX > 0) ? (t.pos.x + box.w + 1.0F) : (t.pos.x - 2.0F);
      const Rect probe{edgeProbeX, t.pos.y + box.h + 1.0F, 2.0F, 2.0F};
      if (!s.overlapsSolid(probe, false)) {
        state.facingX *= -1;
      }
    }

    switch (cfg.type) {
      case EnemyConfig::Type::Walker:
      case EnemyConfig::Type::Spiky:
        v.v.x = static_cast<float>(state.facingX) * cfg.move.speed;
        break;
      case EnemyConfig::Type::Hopper:
        if (g.onGround && state.aiTimerFrames <= 0) {
          v.v.x = static_cast<float>(state.facingX) * cfg.hopper.hopSpeedX;
          v.v.y = cfg.hopper.hopSpeedY;
          state.aiTimerFrames = std::max(0, cfg.hopper.intervalFrames);
        } else {
          v.v.x = static_cast<float>(state.facingX) * cfg.move.speed;
        }
        break;
      case EnemyConfig::Type::Shooter:
        v.v.x = 0.0F;
        if (state.attackTimerFrames <= 0) {
          spawnEnemyProjectile(w, cfg, state, t, box);
          state.attackTimerFrames = std::max(1, cfg.shooter.fireIntervalFrames);
        }
        break;
    }

    v.v.y += cfg.move.gravity;
    v.v.y = std::min(v.v.y, cfg.move.maxFallSpeed);
  }
}

void integrate(World& w, Stage& s, TimeStep ts) {
  (void)ts;
  auto view = w.registry.view<Transform, Velocity, AABB, Grounded>();
  for (auto entity : view) {
    auto& t = view.get<Transform>(entity);
    auto& v = view.get<Velocity>(entity);
    const auto& box = view.get<AABB>(entity);
    auto& g = view.get<Grounded>(entity);

    const CharacterConfig* cfg = CharacterController::get(entity);
    if (cfg && cfg->math.subpixel > 1) {
      t.pos.x = util::quantizeToGrid(t.pos.x, cfg->math.subpixel, cfg->math.quantize);
      t.pos.y = util::quantizeToGrid(t.pos.y, cfg->math.subpixel, cfg->math.quantize);
    }

    bool ignoreOneWay = false;
    if (auto* drop = w.registry.try_get<DropThroughFrames>(entity)) {
      ignoreOneWay = drop->value > 0;
    }

    const bool snapCollisionToPixel =
        (cfg && cfg->math.collisionSnap == CharacterConfig::CollisionSnap::Pixel);
    s.resolveAABBCollision(t, v, box, g, ignoreOneWay, snapCollisionToPixel);

    if (cfg && cfg->math.subpixel > 1) {
      t.pos.x = util::quantizeToGrid(t.pos.x, cfg->math.subpixel, cfg->math.quantize);
      t.pos.y = util::quantizeToGrid(t.pos.y, cfg->math.subpixel, cfg->math.quantize);

      v.v.x = util::quantizeToGrid(v.v.x, cfg->math.subpixel, cfg->math.quantize);
      v.v.y = util::quantizeToGrid(v.v.y, cfg->math.subpixel, cfg->math.quantize);
    }
  }
}

void attackHitboxes(World& w, Stage& s, TimeStep ts) {
  (void)s;
  (void)ts;
  std::vector<EntityId> toDestroy;

  auto view = w.registry.view<AttackHitboxTag, Transform, AABB, AttackHitboxState>();
  for (auto entity : view) {
    auto& t = view.get<Transform>(entity);
    const auto& box = view.get<AABB>(entity);
    auto& state = view.get<AttackHitboxState>(entity);

    if (state.owner != kInvalidEntity && w.registry.valid(state.owner)) {
      if (!w.registry.all_of<Transform, AABB>(state.owner)) {
        toDestroy.push_back(entity);
        continue;
      }

      const auto& ownerT = w.registry.get<Transform>(state.owner);
      const auto& ownerB = w.registry.get<AABB>(state.owner);

      int facingX = 1;
      if (auto* ownerA = w.registry.try_get<ActionState>(state.owner)) {
        facingX = (ownerA->facingX < 0) ? -1 : 1;
      }
      const float facing = static_cast<float>(facingX);
      const float centerX = ownerT.pos.x + ownerB.w * 0.5F;
      const float centerY = ownerT.pos.y + ownerB.h * 0.5F;
      t.pos.x = centerX + state.offsetX * facing - box.w * 0.5F;
      t.pos.y = centerY + state.offsetY - box.h * 0.5F;
    }

    if (state.startupFrames > 0) {
      --state.startupFrames;
    } else if (state.activeFrames > 0) {
      --state.activeFrames;
      if (state.activeFrames <= 0) {
        toDestroy.push_back(entity);
      }
    } else {
      toDestroy.push_back(entity);
    }
  }

  for (EntityId id : toDestroy) {
    w.destroy(id);
  }
}

void projectiles(World& w, Stage& s, TimeStep ts) {
  (void)ts;
  std::vector<EntityId> toDestroy;

  auto view = w.registry.view<ProjectileTag, Transform, Velocity, AABB, ProjectileState>();
  for (auto entity : view) {
    auto& t = view.get<Transform>(entity);
    auto& v = view.get<Velocity>(entity);
    const auto& box = view.get<AABB>(entity);
    auto& state = view.get<ProjectileState>(entity);

    if (state.lifetimeFrames > 0) {
      --state.lifetimeFrames;
      if (state.lifetimeFrames <= 0) {
        toDestroy.push_back(entity);
        continue;
      }
    }

    v.v.y += state.gravity;
    t.pos.x += v.v.x;
    t.pos.y += v.v.y;

    const Rect rect{t.pos.x, t.pos.y, box.w, box.h};
    if (s.overlapsSolid(rect, false)) {
      toDestroy.push_back(entity);
    }
  }

  for (EntityId id : toDestroy) {
    w.destroy(id);
  }
}

void hazards(World& w, Stage& s, TimeStep ts) {
  (void)ts;  // Frame-based timers don't need delta time

  auto view = w.registry.view<CharacterTag, Transform, Velocity, AABB>();
  for (auto entity : view) {
    auto& t = view.get<Transform>(entity);
    auto& v = view.get<Velocity>(entity);
    const auto& box = view.get<AABB>(entity);

    auto& invTimer = w.registry.get_or_emplace<TimerFrames>(entity, TimerFrames{0});
    auto& lockout = w.registry.get_or_emplace<HurtLockoutFrames>(entity, HurtLockoutFrames{0});

    int& invFrames = invTimer.value;
    if (invFrames > 0)
      --invFrames;

    if (lockout.value > 0)
      --lockout.value;

    const Rect aabbRect{t.pos.x, t.pos.y, box.w, box.h};
    StageHazard hazard{};
    if (!s.hazardAt(aabbRect, hazard))
      continue;

    if (invFrames > 0 && !hazard.ignoreIframes)
      continue;

    invFrames = msToFrames(hazard.iframesMs);
    lockout.value = std::max(lockout.value, msToFrames(hazard.lockoutMs));
    ++w.hurtEvents;

    const float playerCx = aabbRect.x + aabbRect.w * 0.5F;
    const float hazardCx = hazard.rect.x + hazard.rect.w * 0.5F;
    const int awayX = (playerCx < hazardCx) ? -1 : 1;
    v.v.x = std::fabs(hazard.knockbackVx) * static_cast<float>(awayX);
    v.v.y = hazard.knockbackVy;

    if (auto* g = w.registry.try_get<Grounded>(entity)) {
      if (hazard.knockbackVy < 0.0F)
        g->onGround = false;
    }
  }
}

void collectibles(World& w, Stage& s, TimeStep ts) {
  (void)s;
  (void)ts;

  const EntityId player = w.player;
  if (player == kInvalidEntity || !w.registry.valid(player))
    return;

  if (!w.registry.all_of<Transform, AABB>(player))
    return;

  const auto& pT = w.registry.get<Transform>(player);
  const auto& pB = w.registry.get<AABB>(player);
  const float px = pT.pos.x;
  const float py = pT.pos.y;
  const float pw = pB.w;
  const float ph = pB.h;

  std::vector<EntityId> toCollect;
  auto view = w.registry.view<CollectibleTag, Transform, AABB>();
  for (auto entity : view) {
    const auto& t = view.get<Transform>(entity);
    const auto& box = view.get<AABB>(entity);

    const float cx = t.pos.x;
    const float cy = t.pos.y;
    const float cw = box.w;
    const float ch = box.h;

    if (aabbOverlap(px, py, pw, ph, cx, cy, cw, ch)) {
      toCollect.push_back(entity);
    }
  }

  for (EntityId id : toCollect) {
    w.destroy(id);
  }
}

void combat(World& w, Stage& s, TimeStep ts) {
  (void)s;
  (void)ts;

  const EntityId player = w.player;
  if (player == kInvalidEntity || !w.registry.valid(player)) {
    return;
  }

  if (!w.registry.all_of<Transform, Velocity, AABB, Grounded>(player)) {
    return;
  }

  auto& pT = w.registry.get<Transform>(player);
  auto& pV = w.registry.get<Velocity>(player);
  const auto& pB = w.registry.get<AABB>(player);
  auto& pG = w.registry.get<Grounded>(player);

  auto& invTimer = w.registry.get_or_emplace<TimerFrames>(player, TimerFrames{0});
  int& invMs = invTimer.value;

  const float playerBottom = pT.pos.y + pB.h;
  const float playerPrevBottom = playerBottom - pV.v.y;

  std::vector<EntityId> toDestroy;
  std::vector<EntityId> projectilesToDestroy;
  std::vector<EntityId> hitboxesToDestroy;
  auto markEnemyDestroyed = [&](EntityId id) {
    if (std::find(toDestroy.begin(), toDestroy.end(), id) == toDestroy.end()) {
      toDestroy.push_back(id);
    }
  };

  // Enemy-player collision
  auto enemyView =
      w.registry.view<EnemyTag, Transform, AABB, EnemyState, EnemyConfig, Velocity, Grounded>();
  for (auto entity : enemyView) {
    const auto& eT = enemyView.get<Transform>(entity);
    const auto& eB = enemyView.get<AABB>(entity);
    auto& estate = enemyView.get<EnemyState>(entity);
    const auto& cfg = enemyView.get<EnemyConfig>(entity);
    (void)enemyView.get<Velocity>(entity);  // Reserved for future knockback
    (void)enemyView.get<Grounded>(entity);  // Reserved for future ground-only attacks

    const Rect enemyRect{eT.pos.x, eT.pos.y, eB.w, eB.h};
    const Rect playerRect{pT.pos.x, pT.pos.y, pB.w, pB.h};

    if (!aabbOverlap(playerRect.x, playerRect.y, playerRect.w, playerRect.h, enemyRect.x,
                     enemyRect.y, enemyRect.w, enemyRect.h)) {
      continue;
    }

    const bool falling = pV.v.y > 0.0F;
    const bool fromAbove = playerPrevBottom <= enemyRect.y + 6.0F;
    const bool canStomp = cfg.combat.stompable && falling && fromAbove;

    if (canStomp && estate.invincibleMs <= 0) {
      estate.health = std::max(0, estate.health - 1);
      estate.invincibleMs = std::max(estate.invincibleMs, cfg.combat.iframesMs);

      pV.v.y = cfg.combat.stompBounceVy;
      pG.onGround = false;

      if (estate.health <= 0) {
        markEnemyDestroyed(entity);
      }
      continue;
    }

    if (invMs > 0) {
      continue;
    }

    invMs = std::max(invMs, msToFrames(cfg.combat.iframesMs));
    ++w.hurtEvents;

    const float playerCx = playerRect.x + playerRect.w * 0.5F;
    const float enemyCx = enemyRect.x + enemyRect.w * 0.5F;
    const int awayX = (playerCx < enemyCx) ? -1 : 1;
    pV.v.x = std::fabs(cfg.combat.knockbackVx) * static_cast<float>(awayX);
    pV.v.y = cfg.combat.knockbackVy;
    if (cfg.combat.knockbackVy < 0.0F) {
      pG.onGround = false;
    }
  }

  for (EntityId id : toDestroy) {
    w.destroy(id);
    ++w.enemyKills;
  }
  toDestroy.clear();

  // Attack hitbox vs enemy
  const Rect playerRect{pT.pos.x, pT.pos.y, pB.w, pB.h};
  auto hitboxView = w.registry.view<AttackHitboxTag, Transform, AABB, AttackHitboxState>();
  for (auto hEntity : hitboxView) {
    const auto& hT = hitboxView.get<Transform>(hEntity);
    const auto& hB = hitboxView.get<AABB>(hEntity);
    auto& hstate = hitboxView.get<AttackHitboxState>(hEntity);

    if (!hstate.fromPlayer) {
      continue;
    }
    if (hstate.startupFrames > 0 || hstate.activeFrames <= 0) {
      continue;
    }

    const Rect hitRect{hT.pos.x, hT.pos.y, hB.w, hB.h};
    bool hitSomething = false;

    for (auto eEntity : enemyView) {
      const auto& eT = enemyView.get<Transform>(eEntity);
      const auto& eB = enemyView.get<AABB>(eEntity);
      auto& estate = enemyView.get<EnemyState>(eEntity);
      const auto& ecfg = enemyView.get<EnemyConfig>(eEntity);
      auto& eV = enemyView.get<Velocity>(eEntity);
      auto& eG = enemyView.get<Grounded>(eEntity);

      if (estate.invincibleMs > 0) {
        continue;
      }

      const Rect enemyRect{eT.pos.x, eT.pos.y, eB.w, eB.h};
      if (!aabbOverlap(hitRect.x, hitRect.y, hitRect.w, hitRect.h, enemyRect.x, enemyRect.y,
                       enemyRect.w, enemyRect.h)) {
        continue;
      }

      const int damage = std::max(1, static_cast<int>(std::round(hstate.damage)));
      estate.health = std::max(0, estate.health - damage);
      estate.invincibleMs = std::max(estate.invincibleMs, ecfg.combat.iframesMs);

      const float enemyCx = enemyRect.x + enemyRect.w * 0.5F;
      const float hitCx = hitRect.x + hitRect.w * 0.5F;
      const int awayX = (enemyCx < hitCx) ? -1 : 1;
      eV.v.x = std::fabs(hstate.knockbackVx) * static_cast<float>(awayX);
      eV.v.y = hstate.knockbackVy;
      if (hstate.knockbackVy < 0.0F) {
        eG.onGround = false;
      }

      if (estate.health <= 0) {
        markEnemyDestroyed(eEntity);
      }
      hitSomething = true;
      break;
    }

    if (hitSomething) {
      hstate.hitsRemaining = std::max(0, hstate.hitsRemaining - 1);
      if (hstate.hitsRemaining <= 0) {
        hitboxesToDestroy.push_back(hEntity);
      }
    }
  }

  for (EntityId id : toDestroy) {
    w.destroy(id);
    ++w.enemyKills;
  }
  toDestroy.clear();

  // Enemy projectiles vs player
  auto projView = w.registry.view<ProjectileTag, Transform, AABB, ProjectileState>();
  for (auto pEntity : projView) {
    const auto& projT = projView.get<Transform>(pEntity);
    const auto& projB = projView.get<AABB>(pEntity);
    const auto& pstate = projView.get<ProjectileState>(pEntity);

    if (!pstate.fromEnemy) {
      continue;
    }

    const Rect projRect{projT.pos.x, projT.pos.y, projB.w, projB.h};
    if (!aabbOverlap(playerRect.x, playerRect.y, playerRect.w, playerRect.h, projRect.x, projRect.y,
                     projRect.w, projRect.h)) {
      continue;
    }

    projectilesToDestroy.push_back(pEntity);
    if (invMs > 0) {
      continue;
    }

    invMs = std::max(invMs, msToFrames(pstate.iframesMs));
    ++w.hurtEvents;

    const float playerCx = playerRect.x + playerRect.w * 0.5F;
    const float projCx = projRect.x + projRect.w * 0.5F;
    const int awayX = (playerCx < projCx) ? -1 : 1;
    pV.v.x = std::fabs(pstate.knockbackVx) * static_cast<float>(awayX);
    pV.v.y = pstate.knockbackVy;
    if (pstate.knockbackVy < 0.0F) {
      pG.onGround = false;
    }
  }

  // Player projectiles vs enemies
  for (auto pEntity : projView) {
    const auto& projT = projView.get<Transform>(pEntity);
    const auto& projB = projView.get<AABB>(pEntity);
    auto& pstate = projView.get<ProjectileState>(pEntity);

    if (pstate.fromEnemy) {
      continue;
    }

    const Rect projRect{projT.pos.x, projT.pos.y, projB.w, projB.h};
    bool hitSomething = false;

    for (auto eEntity : enemyView) {
      const auto& eT = enemyView.get<Transform>(eEntity);
      const auto& eB = enemyView.get<AABB>(eEntity);
      auto& estate = enemyView.get<EnemyState>(eEntity);
      const auto& ecfg = enemyView.get<EnemyConfig>(eEntity);
      auto& eV = enemyView.get<Velocity>(eEntity);
      auto& eG = enemyView.get<Grounded>(eEntity);

      if (estate.invincibleMs > 0) {
        continue;
      }

      const Rect enemyRect{eT.pos.x, eT.pos.y, eB.w, eB.h};
      if (!aabbOverlap(projRect.x, projRect.y, projRect.w, projRect.h, enemyRect.x, enemyRect.y,
                       enemyRect.w, enemyRect.h)) {
        continue;
      }

      const int damage = std::max(1, static_cast<int>(std::round(pstate.damage)));
      estate.health = std::max(0, estate.health - damage);
      estate.invincibleMs = std::max(estate.invincibleMs, ecfg.combat.iframesMs);

      const float enemyCx = enemyRect.x + enemyRect.w * 0.5F;
      const float projCx = projRect.x + projRect.w * 0.5F;
      const int awayX = (enemyCx < projCx) ? -1 : 1;
      eV.v.x = std::fabs(pstate.knockbackVx) * static_cast<float>(awayX);
      eV.v.y = pstate.knockbackVy;
      if (pstate.knockbackVy < 0.0F) {
        eG.onGround = false;
      }

      if (estate.health <= 0) {
        markEnemyDestroyed(eEntity);
      }
      hitSomething = true;
      break;
    }

    if (hitSomething) {
      projectilesToDestroy.push_back(pEntity);
    }
  }

  for (EntityId id : projectilesToDestroy) {
    w.destroy(id);
  }

  for (EntityId id : hitboxesToDestroy) {
    w.destroy(id);
  }

  for (EntityId id : toDestroy) {
    w.destroy(id);
    ++w.enemyKills;
  }
}

void deriveAnimState(World& w) {
  static constexpr float kRunSpeedEpsilon = 5.0F;  // px/s

  auto view = w.registry.view<AnimState, Velocity, Grounded>();
  for (auto entity : view) {
    auto& anim = view.get<AnimState>(entity);
    const auto& v = view.get<Velocity>(entity);
    const auto& g = view.get<Grounded>(entity);

    if (auto* a = w.registry.try_get<ActionState>(entity)) {
      if (a->attackAnimFrames > 0 && a->attackAnimKind != AttackAnimKind::None) {
        anim.id = (a->attackAnimKind == AttackAnimKind::Shoot) ? AnimStateId::AttackShoot
                                                               : AnimStateId::AttackMelee;
        continue;
      }
      if (a->spindashCharging && g.onGround) {
        anim.id = AnimStateId::SpindashCharge;
        continue;
      }
      if (a->dashing) {
        anim.id = AnimStateId::Dash;
        continue;
      }
      if (a->flying && !g.onGround) {
        anim.id = AnimStateId::Fly;
        continue;
      }
      if (a->gliding && !g.onGround) {
        anim.id = AnimStateId::Glide;
        continue;
      }
      if (a->spinning) {
        anim.id = AnimStateId::Spin;
        continue;
      }
    }

    if (g.onGround) {
      anim.id = (std::fabs(v.v.x) <= kRunSpeedEpsilon) ? AnimStateId::Idle : AnimStateId::Run;
    } else {
      anim.id = (v.v.y < 0.0F) ? AnimStateId::Jump : AnimStateId::Fall;
    }
  }
}

}  // namespace Systems
