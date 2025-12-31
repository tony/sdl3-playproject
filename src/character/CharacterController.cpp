#include "character/CharacterController.h"

#include <algorithm>
#include <cmath>
#include <string_view>
#include <utility>
#include <vector>

#include "character/Actions.h"
#include "core/Time.h"
#include "ecs/Components.h"
#include "ecs/World.h"
#include "stage/Stage.h"
#include "util/Math.h"

// Treat some config fields as dimensionless "feel" knobs and scale them into px/frame.
// At 120Hz frame-based: 400/14400 = 0.0278 px/frame per unit friction.
static constexpr float kAirDragToDecelScale = 400.0F / 14400.0F;
static constexpr float kGroundFrictionToDecelScale = 400.0F / 14400.0F;
static constexpr float kSpinFrictionToDecelScale = 400.0F / 14400.0F;
static constexpr int kDashWallKickDisplayFrames = 14;  // 120ms at 120Hz

static float approach(float v, float target, float delta) {
  if (v < target) {
    return std::min(v + delta, target);
  }
  return std::max(v - delta, target);
}

static int signOrZero(float v) {
  if (v > 0.0F) {
    return 1;
  }
  if (v < 0.0F) {
    return -1;
  }
  return 0;
}

static int signOrFacing(float v, int facingX) {
  const int sign = signOrZero(v);
  return (sign != 0) ? sign : facingX;
}

// Frame-based physics: timers decrement by 1 each frame.
// No conversion needed - values are already in frames.

// Adjust hitbox dimensions and compensate position to keep feet grounded.
// Returns true if dimensions actually changed.
static bool applyHitboxChange(AABB& box, Transform& t, float newW, float newH) {
  if (newW <= 0.0F || newH <= 0.0F) {
    return false;  // Invalid dimensions
  }
  if (box.w == newW && box.h == newH) {
    return false;  // No change
  }
  // Adjust Y to keep feet at same level (top-left anchor system).
  // Feet are at pos.y + h, so when h shrinks, pos.y must increase.
  const float heightDiff = box.h - newH;
  t.pos.y += heightDiff;
  box.w = newW;
  box.h = newH;
  return true;
}

static float resolveJumpImpulse(const CharacterConfig& cfg, float absVx) {
  float impulse = cfg.jump.impulse;
  for (const auto& e : cfg.jump.impulseBySpeed) {
    if (absVx >= e.minAbsVx) {
      impulse = e.impulse;
    }
  }
  return impulse;
}

static void actionButton(const InputState& in,
                         ActionInput input,
                         bool& outPressed,
                         bool& outHeld,
                         bool& outReleased) {
  outPressed = false;
  outHeld = false;
  outReleased = false;

  switch (input) {
    case ActionInput::Action1:
      outPressed = in.action1Pressed;
      outHeld = in.action1Held;
      outReleased = in.action1Released;
      break;
    case ActionInput::Action2:
      outPressed = in.action2Pressed;
      outHeld = in.action2Held;
      outReleased = in.action2Released;
      break;
    case ActionInput::Jump:
      outPressed = in.jumpPressed;
      outHeld = in.jumpHeld;
      outReleased = in.jumpReleased;
      break;
  }
}

static int actionPressedFrames(const InputState& in, ActionInput input) {
  switch (input) {
    case ActionInput::Action1:
      return in.action1PressedFrames;
    case ActionInput::Action2:
      return in.action2PressedFrames;
    case ActionInput::Jump:
      return in.jumpPressedFrames;
  }
  return InputState::kUnpressedFrames;
}

static int actionHeldFrames(const InputState& in, ActionInput input) {
  switch (input) {
    case ActionInput::Action1:
      return in.action1HeldFrames;
    case ActionInput::Action2:
      return in.action2HeldFrames;
    case ActionInput::Jump:
      return in.jumpHeldFrames;
  }
  return 0;
}

static void allowAirJumpOverride(const InputState& in,
                                 const Grounded& g,
                                 ActionInput input,
                                 bool allowOverride,
                                 bool& outPressed,
                                 bool& outHeld,
                                 bool& outReleased) {
  if (!allowOverride || input == ActionInput::Jump) {
    return;
  }
  if (g.onGround) {
    return;
  }
  outPressed = outPressed || in.jumpPressed;
  outHeld = outHeld || in.jumpHeld;
  outReleased = outReleased || in.jumpReleased;
}

static bool hasPowerup(const CharacterConfig& cfg, std::string_view powerupId) {
  if (powerupId.empty()) {
    return true;
  }
  for (const auto& powerup : cfg.powerups) {
    if (powerup == powerupId) {
      return true;
    }
  }
  return false;
}

static void spawnAttackHitbox(World& w, EntityId owner, const AttackAction& attack, int facingX) {
  if (!w.registry.valid(owner) || !w.registry.all_of<Transform, AABB>(owner)) {
    return;
  }
  if (attack.hitbox.w <= 0.0F || attack.hitbox.h <= 0.0F) {
    return;
  }

  const auto& ownerT = w.registry.get<Transform>(owner);
  const auto& ownerB = w.registry.get<AABB>(owner);

  const float facing = static_cast<float>((facingX < 0) ? -1 : 1);
  const float centerX = ownerT.pos.x + ownerB.w * 0.5F;
  const float centerY = ownerT.pos.y + ownerB.h * 0.5F;
  const float spawnX = centerX + attack.hitbox.offsetX * facing - attack.hitbox.w * 0.5F;
  const float spawnY = centerY + attack.hitbox.offsetY - attack.hitbox.h * 0.5F;

  const EntityId e = w.create();
  w.registry.emplace<Transform>(e, Vec2{spawnX, spawnY});
  w.registry.emplace<AABB>(e, attack.hitbox.w, attack.hitbox.h);
  w.registry.emplace<AttackHitboxTag>(e);

  AttackHitboxState state{};
  state.owner = owner;
  state.startupFrames = std::max(0, attack.startupFrames);
  state.activeFrames = std::max(0, attack.activeFrames);
  state.hitsRemaining = std::max(1, attack.hits);
  state.damage = attack.damage;
  state.knockbackVx = attack.knockbackVx;
  state.knockbackVy = attack.knockbackVy;
  state.offsetX = attack.hitbox.offsetX;
  state.offsetY = attack.hitbox.offsetY;
  state.fromPlayer = true;
  w.registry.emplace<AttackHitboxState>(e, state);
}

static void spawnAttackProjectile(World& w,
                                  EntityId owner,
                                  const AttackAction& attack,
                                  int facingX) {
  if (!attack.projectile.enabled) {
    return;
  }

  if (!w.registry.valid(owner) || !w.registry.all_of<Transform, AABB>(owner)) {
    return;
  }

  const auto& ownerT = w.registry.get<Transform>(owner);
  const auto& ownerB = w.registry.get<AABB>(owner);

  const auto& proj = attack.projectile;
  const float facing = static_cast<float>((facingX < 0) ? -1 : 1);
  const float centerX = ownerT.pos.x + ownerB.w * 0.5F;
  const float centerY = ownerT.pos.y + ownerB.h * 0.5F;
  const float spawnX = centerX + proj.offsetX * facing - proj.w * 0.5F;
  const float spawnY = centerY + proj.offsetY - proj.h * 0.5F;

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
  pstate.iframesMs = 0;
  pstate.fromEnemy = false;
  w.registry.emplace<ProjectileState>(e, pstate);
}

void CharacterController::attach(EntityId e, CharacterConfig cfg) {
  rt_[e] = Runtime{};
  auto& rt = rt_[e];
  rt.cfg = std::move(cfg);
  rt.attacks.resize(rt.cfg.actions.attacks.size());
}

void CharacterController::detach(EntityId e) {
  rt_.erase(e);
}

void CharacterController::resetRuntime(EntityId e) {
  auto it = rt_.find(e);
  if (it == rt_.end()) {
    return;
  }
  auto& rt = it->second;
  rt.attacks.resize(rt.cfg.actions.attacks.size());
  for (auto& attack : rt.attacks) {
    attack.heldLatch = false;
  }
  rt.dashRemainingFrames = 0;
  rt.jumpCutApplied = false;
  rt.jumpHeldFrames = 0;
  rt.jumpReleaseDropApplied = false;
  rt.airDashesUsed = 0;
  rt.spindashCharging = false;
  rt.spindashChargeFrames = 0;
  rt.spinning = false;
  rt.spinFacingX = 1;
  rt.rolling = false;
  rt.rollJumpNoAirControl = false;
  rt.sliding = false;
  rt.slideRemainingFrames = 0;
  rt.slideJumpLockoutFrames = 0;
  rt.hitboxReduced = false;
  rt.hitboxFromSlide = false;
  rt.glideActive = false;
  rt.wallDetachFrames = 0;
  rt.dashWallKickFrames = 0;
  rt.attackAnimFrames = 0;
  rt.attackAnimTotalFrames = 0;
  rt.attackAnimKind = AttackAnimKind::None;
}

void CharacterController::setFacing(EntityId e, int facingX) {
  auto it = rt_.find(e);
  if (it == rt_.end()) {
    return;
  }
  it->second.facingX = (facingX < 0) ? -1 : 1;
  it->second.spinFacingX = it->second.facingX;
}

bool CharacterController::replaceConfig(EntityId e, CharacterConfig cfg, bool resetRuntimeState) {
  auto it = rt_.find(e);
  if (it == rt_.end()) {
    return false;
  }

  it->second.cfg = std::move(cfg);
  it->second.attacks.resize(it->second.cfg.actions.attacks.size());
  if (resetRuntimeState) {
    resetRuntime(e);
  }

  return true;
}

const CharacterConfig* CharacterController::get(EntityId e) {
  auto it = rt_.find(e);
  if (it == rt_.end()) {
    return nullptr;
  }
  return &it->second.cfg;
}

// Keep controller logic in one pass.
// NOLINTNEXTLINE
void CharacterController::tick(World& w, Stage& s, TimeStep ts) {
  (void)ts;

  auto view = w.registry.view<CharacterTag>();
  for (auto id : view) {
    auto itRt = rt_.find(id);
    if (itRt == rt_.end()) {
      continue;
    }

    auto& rt = itRt->second;
    auto& cfg = rt.cfg;

    // Check required components exist
    if (!w.registry.all_of<Transform, Velocity, AABB, InputState, Grounded, ActionState>(id)) {
      continue;
    }

    auto& t = w.registry.get<Transform>(id);
    auto& v = w.registry.get<Velocity>(id);
    auto& box = w.registry.get<AABB>(id);
    auto& in = w.registry.get<InputState>(id);
    auto& g = w.registry.get<Grounded>(id);
    auto& as = w.registry.get<ActionState>(id);

    // Get or create coyote timer component
    auto& coyoteTimer = w.registry.get_or_emplace<CoyoteFrames>(id, CoyoteFrames{0});
    int& coyote = coyoteTimer.value;

    const Rect aabbRect{t.pos.x, t.pos.y, box.w, box.h};
    StageEnvironmentMultipliers env = s.environmentAt(aabbRect);
    auto applyEnvironment = [&](const CharacterConfig::EnvironmentMultipliers& mult) {
      env.gravity *= mult.gravity;
      env.maxFallSpeed *= mult.maxFallSpeed;
      env.accel *= mult.accel;
      env.maxSpeed *= mult.maxSpeed;
      env.friction *= mult.friction;
      env.groundFriction *= mult.groundFriction;
      env.airDrag *= mult.airDrag;
      env.turnResistance *= mult.turnResistance;
      env.jumpImpulse *= mult.jumpImpulse;
    };
    if (env.inWater) {
      applyEnvironment(cfg.environment.water);
    }
    if (env.inIce) {
      applyEnvironment(cfg.environment.ice);
    }

    if (g.onGround) {
      rt.airDashesUsed = 0;
      rt.dashCarry = false;
      rt.dashCarrySpeed = 0.0F;
      rt.jumpHeldFrames = 0;
      rt.jumpReleaseDropApplied = false;
      rt.rollJumpNoAirControl = false;  // restore air control on landing
    }

    // timers (frame-based: decrement by 1 each frame)
    // Simplified timer handling - using runtime state
    int& jbuf = rt.jumpBufferFrames;
    int& acd = rt.actionCooldownFrames;
    int& atkCd = rt.attackCooldownFrames;
    int& dropThrough = rt.dropThroughFrames;
    int& lockout = rt.hurtLockoutFrames;

    // Sync external lockout updates from hazard system into runtime
    if (auto* extLock = w.registry.try_get<HurtLockoutFrames>(id)) {
      if (extLock->value > lockout) {
        lockout = extLock->value;
      }
    }

    if (g.onGround) {
      coyote = cfg.jump.coyoteFrames;
    } else if (coyote > 0) {
      --coyote;
    }

    if (lockout > 0) {
      --lockout;
    }
    const bool controlsLocked = (lockout > 0);

    if (controlsLocked) {
      rt.dashRemainingFrames = 0;
      rt.spindashCharging = false;
      rt.spindashChargeFrames = 0;
      rt.spinning = false;
      rt.rolling = false;
      rt.rollJumpNoAirControl = false;
      rt.sliding = false;
      rt.slideRemainingFrames = 0;
      rt.slideJumpLockoutFrames = 0;
      rt.glideActive = false;
      rt.dashCarry = false;
      rt.dashCarrySpeed = 0.0F;
      rt.dashWallKickFrames = 0;
      rt.attackAnimFrames = 0;
      rt.attackAnimTotalFrames = 0;
      rt.attackAnimKind = AttackAnimKind::None;
    }

    if (rt.wallDetachFrames > 0) {
      --rt.wallDetachFrames;
    }
    if (rt.dashWallKickFrames > 0) {
      --rt.dashWallKickFrames;
    }

    InputState ctl = in;
    if (controlsLocked) {
      ctl.left = false;
      ctl.right = false;
      ctl.upPressed = false;
      ctl.upHeld = false;
      ctl.upReleased = false;
      ctl.downPressed = false;
      ctl.downHeld = false;
      ctl.downReleased = false;
      ctl.jumpPressed = false;
      ctl.jumpReleased = false;
      ctl.action1Pressed = false;
      ctl.action1Held = false;
      ctl.action1Released = false;
      ctl.action2Pressed = false;
      ctl.action2Held = false;
      ctl.action2Released = false;
      ctl.upHeldFrames = 0;
      ctl.upPressedFrames = InputState::kUnpressedFrames;
      ctl.downHeldFrames = 0;
      ctl.downPressedFrames = InputState::kUnpressedFrames;
      ctl.jumpHeldFrames = 0;
      ctl.jumpPressedFrames = InputState::kUnpressedFrames;
      ctl.action1HeldFrames = 0;
      ctl.action1PressedFrames = InputState::kUnpressedFrames;
      ctl.action2HeldFrames = 0;
      ctl.action2PressedFrames = InputState::kUnpressedFrames;
    }

    const bool spindashEnabled = cfg.actions.spindash.enabled;
    const bool spindashUsesJump =
        spindashEnabled && (cfg.actions.spindash.input == ActionInput::Jump);
    const bool spindashRequireDown = spindashEnabled && cfg.actions.spindash.requireDown;
    const int spindashDownWindowFrames = std::max(0, cfg.actions.spindash.downWindowFrames);
    const bool spindashJumpWindowActive =
        spindashUsesJump && spindashRequireDown && spindashDownWindowFrames > 0 && !ctl.downHeld &&
        ctl.jumpHeld && ctl.jumpPressedFrames <= spindashDownWindowFrames;

    if (ctl.jumpPressed) {
      jbuf =
          (spindashUsesJump && spindashRequireDown && ctl.downHeld) ? 0 : cfg.jump.jumpBufferFrames;
    } else if (jbuf > 0) {
      --jbuf;
    }

    if (acd > 0) {
      --acd;
    }
    if (atkCd > 0) {
      --atkCd;
    }
    if (dropThrough > 0) {
      --dropThrough;
    }
    if (rt.attackAnimFrames > 0) {
      --rt.attackAnimFrames;
      if (rt.attackAnimFrames <= 0) {
        rt.attackAnimFrames = 0;
        rt.attackAnimTotalFrames = 0;
        rt.attackAnimKind = AttackAnimKind::None;
      }
    }

    // Slide/Drop-through: Down+Jump while grounded.
    // Context-sensitive: one-way platform → drop-through, solid ground → slide (if enabled).
    const bool onOneWay = s.groundIsOneWay(aabbRect);
    bool dropThroughAllowed = true;
    if (spindashUsesJump && spindashRequireDown) {
      dropThroughAllowed = onOneWay;
    }
    const int dropThroughHoldFrames = std::max(0, cfg.states.dropThrough.holdFrames);
    // Slide takes priority on solid ground (if enabled)
    const bool slideEnabled = cfg.states.slide.enabled;
    const bool canStartSlide = slideEnabled && g.onGround && !onOneWay && !rt.sliding;
    const int slideWindow = std::max(0, cfg.states.slide.inputWindowFrames);
    bool slideInput = false;
    if (slideWindow == 0) {
      slideInput = ctl.downHeld && ctl.jumpPressed;
    } else {
      const bool jumpRecent = ctl.jumpPressedFrames <= slideWindow;
      const bool downRecent = ctl.downPressedFrames <= slideWindow;
      slideInput = (ctl.downHeld && jumpRecent) || (ctl.jumpHeld && downRecent);
    }
    const bool dropThroughInput = ctl.downHeld && ctl.jumpPressed;
    if (g.onGround && slideInput) {
      if (canStartSlide) {
        // Start slide (Mega Man-style)
        rt.sliding = true;
        rt.slideRemainingFrames = cfg.states.slide.durationFrames;
        rt.slideJumpLockoutFrames = cfg.states.slide.jumpLockoutFrames;
        rt.slideDirX = rt.facingX;
        jbuf = 0;  // consume jump buffer
      } else if (dropThroughAllowed && dropThroughInput) {
        // Drop through one-way platform
        dropThrough = std::max(dropThrough, dropThroughHoldFrames);
        coyote = 0;
        jbuf = 0;
        g.onGround = false;
      }
    }

    // Slide state management
    if (rt.sliding) {
      if (rt.slideRemainingFrames > 0)
        --rt.slideRemainingFrames;
      if (rt.slideJumpLockoutFrames > 0)
        --rt.slideJumpLockoutFrames;

      // Cancel on ground loss (MM-accurate)
      if (!g.onGround && cfg.states.slide.cancelOnGroundLoss) {
        rt.sliding = false;
        rt.slideRemainingFrames = 0;
        rt.slideJumpLockoutFrames = 0;
      }

      // Timer expired
      if (rt.slideRemainingFrames <= 0) {
        rt.sliding = false;
        rt.slideJumpLockoutFrames = 0;
      }

      // Jump cancel (after lockout period)
      if (cfg.states.slide.allowJumpCancel && rt.slideJumpLockoutFrames <= 0 && ctl.jumpPressed) {
        rt.sliding = false;
        rt.slideRemainingFrames = 0;
        // Note: jump will be processed in jump resolution below
      }
    }

    // horizontal intent
    float rawWish = 0.0F;
    if (ctl.left) {
      rawWish -= 1.0F;
    }
    if (ctl.right) {
      rawWish += 1.0F;
    }

    float wish = rawWish;
    if (!g.onGround && !cfg.move.airControl) {
      wish = 0.0F;
    }
    // Roll-jump disables air control (Sonic-accurate)
    if (!g.onGround && rt.rollJumpNoAirControl) {
      wish = 0.0F;
    }

    if (wish > 0.0F) {
      rt.facingX = 1;
    }
    if (wish < 0.0F) {
      rt.facingX = -1;
    }

    const bool rollingGround = rt.rolling && g.onGround;
    float maxSpd = g.onGround ? cfg.move.maxSpeedGround : cfg.move.maxSpeedAir;
    if (rollingGround) {
      maxSpd *= std::max(0.0F, cfg.states.rolling.maxSpeedMultiplier);
    }
    maxSpd *= env.maxSpeed;

    const bool dashCarryAir = (!g.onGround && rt.dashCarry && cfg.actions.dash.preserveMomentum);
    const int wishDir = signOrZero(wish);
    int carryDir = rt.dashDirX;
    if (carryDir == 0) {
      carryDir = (v.v.x >= 0.0F) ? 1 : -1;
    }
    const bool dashCarrySameDir = dashCarryAir && (wishDir != 0) && (wishDir == carryDir);
    if (dashCarryAir && wishDir != 0 && wishDir != carryDir) {
      rt.dashCarry = false;
      rt.dashCarrySpeed = 0.0F;
    }

    if (wish == 0.0F) {
      if (!dashCarryAir) {
        const float groundFriction =
            std::max(0.0F, cfg.physics.groundFriction) * env.friction * env.groundFriction;
        const float airDrag = std::max(0.0F, cfg.physics.airDrag) * env.friction * env.airDrag;
        float decel = g.onGround ? (groundFriction * kGroundFrictionToDecelScale)
                                 : (airDrag * kAirDragToDecelScale);
        // Rolling uses reduced friction (Sonic-accurate: 0.5x normal)
        if (rt.rolling && g.onGround) {
          decel *= std::clamp(cfg.states.rolling.frictionMultiplier, 0.0F, 2.0F);
        }
        v.v.x = approach(v.v.x, 0.0F, decel);  // decel is in px/frame
      }
    } else if (cfg.move.model == CharacterConfig::MoveModel::Instant) {
      if (dashCarrySameDir) {
        maxSpd = std::max(maxSpd, rt.dashCarrySpeed);
      }
      v.v.x = wish * maxSpd;
    } else {
      // inertia/turn resistance: reduces effective accel when reversing direction
      float dir = 0.0F;
      if (v.v.x > 0.0F) {
        dir = 1.0F;
      } else if (v.v.x < 0.0F) {
        dir = -1.0F;
      }
      bool reversing = (wish != 0.0F && dir != 0.0F && wish != dir);
      float turnResistance = g.onGround ? cfg.move.turnResistance : cfg.move.turnResistanceAir;
      turnResistance *= env.turnResistance;
      turnResistance = std::clamp(turnResistance, 0.0F, 1.0F);
      float turnPenalty = reversing ? (1.0F - turnResistance) : 1.0F;

      if (dashCarrySameDir) {
        maxSpd = std::max(maxSpd, rt.dashCarrySpeed);
      }
      float accel = g.onGround ? cfg.move.accelGround : cfg.move.accelAir;
      if (rollingGround) {
        accel *= std::max(0.0F, cfg.states.rolling.accelMultiplier);
      }
      accel *= turnPenalty * env.accel;

      // When reversing on the ground, brake to a stop first (decel_ground), then accelerate the
      // other direction.
      if (g.onGround && reversing) {
        v.v.x = approach(v.v.x, 0.0F,
                         cfg.move.decelGround * env.friction * env.groundFriction);  // px/frame
        if (v.v.x == 0.0F) {
          v.v.x = approach(v.v.x, wish * maxSpd, accel);  // px/frame
        }
      } else {
        v.v.x = approach(v.v.x, wish * maxSpd, accel);  // px/frame
      }
    }

    // Slide movement: constant speed, overrides normal movement (MM-style)
    if (rt.sliding && g.onGround) {
      v.v.x = static_cast<float>(rt.slideDirX) * cfg.states.slide.slideSpeed;
    }

    // jump resolution (buffer + coyote)
    //
    // Note: allow disabling buffering/coyote via 0ms config values without disabling jumping
    // entirely (i.e. pressing jump should still work on the press frame while grounded).
    const bool canJump = g.onGround || (coyote > 0);
    const bool jumpBlockedBySpindash =
        spindashUsesJump && spindashRequireDown && (ctl.downHeld || spindashJumpWindowActive);
    const bool jumpBlockedByRoll = rt.rolling && !cfg.states.rolling.allowJump;
    const bool jumpBlockedBySlide = rt.sliding && rt.slideJumpLockoutFrames > 0;
    const bool jumpRequested = (ctl.jumpPressed || (jbuf > 0)) && !jumpBlockedBySpindash &&
                               !jumpBlockedByRoll && !jumpBlockedBySlide;
    const bool dashJump =
        cfg.actions.dash.preserveMomentum && (rt.dashRemainingFrames > 0) && g.onGround;
    const bool wasRolling = rt.rolling;
    if (!controlsLocked && cfg.jump.enabled && jumpRequested && canJump) {
      const float impulse = resolveJumpImpulse(cfg, std::fabs(v.v.x));
      v.v.y = -impulse * env.jumpImpulse;
      g.onGround = false;
      coyote = 0;
      jbuf = 0;
      rt.jumpCutApplied = false;
      rt.jumpHeldFrames = 0;
      rt.jumpReleaseDropApplied = false;
      if (dashJump) {
        rt.dashCarry = true;
        rt.dashCarrySpeed = std::max(std::fabs(v.v.x), cfg.actions.dash.dashSpeed);
      }
      // Roll-jump: disable air control (Sonic-accurate)
      if (wasRolling && cfg.states.rolling.disableAirControl) {
        rt.rollJumpNoAirControl = true;
      }
    }

    const bool jumpHeldEffective =
        in.jumpHeld && (cfg.jump.maxHoldFrames <= 0 || rt.jumpHeldFrames < cfg.jump.maxHoldFrames);

    // variable jump (release early modifies rise)
    if (cfg.jump.variableJump && !jumpHeldEffective && v.v.y < 0.0F && !rt.jumpCutApplied) {
      switch (cfg.jump.model) {
        case CharacterConfig::JumpModel::Impulse:
          v.v.y *= cfg.jump.variableCutMultiplier;
          rt.jumpCutApplied = true;
          break;
        case CharacterConfig::JumpModel::ClampVy:
          if (cfg.jump.releaseClampVy > 0.0F) {
            const float minVy = -cfg.jump.releaseClampVy;
            v.v.y = std::max(v.v.y, minVy);
            rt.jumpCutApplied = true;
          }
          break;
        case CharacterConfig::JumpModel::DualGravity:
          // handled continuously via rise gravity multipliers
          break;
        case CharacterConfig::JumpModel::Fixed:
          // ignore early release
          rt.jumpCutApplied = true;
          break;
      }
    }

    if (!rt.jumpReleaseDropApplied && cfg.jump.releaseDropAfterFrames > 0 &&
        cfg.jump.releaseDrop > 0.0F && ctl.jumpReleased && v.v.y < 0.0F &&
        rt.jumpHeldFrames >= cfg.jump.releaseDropAfterFrames) {
      v.v.y += cfg.jump.releaseDrop;
      rt.jumpReleaseDropApplied = true;
    }

    if (!g.onGround && v.v.y < 0.0F) {
      ++rt.jumpHeldFrames;
    }

    bool dashBtnPressed = false;
    bool dashBtnHeld = false;
    bool dashInputConsumed = false;
    if (cfg.actions.dash.enabled) {
      bool dashBtnReleased = false;
      actionButton(ctl, cfg.actions.dash.input, dashBtnPressed, dashBtnHeld, dashBtnReleased);
      (void)dashBtnReleased;
    }
    // wall interaction: optional slide/climb/jump while holding into a wall.
    bool walling = false;
    int wallDirX = 0;
    if (!controlsLocked && cfg.actions.wall.enabled && !g.onGround && rt.wallDetachFrames == 0) {
      int intoX = 0;
      if (cfg.actions.wall.requireInput) {
        intoX = signOrZero(rawWish);
      } else {
        if (rawWish != 0.0F) {
          intoX = signOrZero(rawWish);
        } else if (v.v.x != 0.0F) {
          intoX = signOrZero(v.v.x);
        } else {
          intoX = rt.facingX;
        }
      }
      if (intoX != 0) {
        const float probe = cfg.actions.wall.probe;
        if (probe > 0.0F && s.touchingWall(aabbRect, intoX, probe)) {
          walling = true;
          wallDirX = intoX;
        }
      }
    }

    if (walling && ctl.jumpPressed && cfg.actions.wall.jumpImpulse > 0.0F) {
      bool dashWallKick = false;
      float jumpVx = std::max(0.0F, cfg.actions.wall.jumpVx);
      // X2/X3 style: check if dash is held on the exact frame of jump (no buffering)
      if (cfg.actions.wall.dashKickEnabled && cfg.actions.dash.enabled) {
        if (dashBtnHeld || dashBtnPressed) {
          dashWallKick = true;
          float dashKickSpeed = cfg.actions.wall.dashKickSpeed;
          if (dashKickSpeed <= 0.0F) {
            dashKickSpeed = cfg.actions.dash.dashSpeed;
          }
          jumpVx = std::max(0.0F, dashKickSpeed);
        }
      }
      v.v.y = -cfg.actions.wall.jumpImpulse;
      if (jumpVx > 0.0F) {
        v.v.x = -static_cast<float>(wallDirX) * jumpVx;
      }
      rt.facingX = -wallDirX;
      rt.jumpCutApplied = false;
      rt.jumpHeldFrames = 0;
      rt.jumpReleaseDropApplied = false;
      coyote = 0;
      jbuf = 0;
      rt.wallDetachFrames = std::max(rt.wallDetachFrames, cfg.actions.wall.detachFrames);
      if (dashWallKick && cfg.actions.dash.preserveMomentum) {
        rt.dashDirX = -wallDirX;
        rt.dashCarry = true;
        rt.dashCarrySpeed = std::max(rt.dashCarrySpeed, jumpVx);
      }
      if (dashWallKick) {
        rt.dashWallKickFrames = kDashWallKickDisplayFrames;
      }
      walling = false;
      wallDirX = 0;
    }

    // gravity
    float gravity = cfg.physics.gravity * env.gravity;
    if (v.v.y < 0.0F && cfg.jump.model == CharacterConfig::JumpModel::DualGravity) {
      float mul = cfg.jump.riseGravityMultiplierHeld;
      if (cfg.jump.variableJump && !jumpHeldEffective) {
        mul = cfg.jump.riseGravityMultiplierReleased;
      }
      gravity *= std::max(0.0F, mul);
    }
    if (v.v.y > 0.0F) {
      gravity *= std::max(0.0F, cfg.jump.fallGravityMultiplier);
    }
    if (walling && v.v.y > 0.0F) {
      gravity *= std::clamp(cfg.actions.wall.slideGravityMultiplier, 0.0F, 1.0F);
    }
    v.v.y += gravity;  // gravity is in px/frame²
    float maxFallSpeed = cfg.physics.maxFallSpeed * env.maxFallSpeed;
    if (walling && cfg.actions.wall.slideMaxFallSpeed > 0.0F) {
      maxFallSpeed = std::min(maxFallSpeed, cfg.actions.wall.slideMaxFallSpeed);
    }
    v.v.y = std::min(v.v.y, maxFallSpeed);

    if (walling) {
      if (cfg.actions.wall.climbSpeed > 0.0F && ctl.upHeld) {
        v.v.y = -cfg.actions.wall.climbSpeed;
      } else if (cfg.actions.wall.descendSpeed > 0.0F && ctl.downHeld) {
        v.v.y = cfg.actions.wall.descendSpeed;
      }
    }

    // action: dash
    if (!controlsLocked && !dashInputConsumed && cfg.actions.dash.enabled && acd == 0) {
      bool btnPressed = false;
      bool btnHeld = false;
      bool btnReleased = false;
      actionButton(ctl, cfg.actions.dash.input, btnPressed, btnHeld, btnReleased);
      allowAirJumpOverride(ctl, g, cfg.actions.dash.input, cfg.actions.dash.allowJumpOverride,
                           btnPressed, btnHeld, btnReleased);

      bool trig = (cfg.actions.dash.trigger == ActionTrigger::Press) ? btnPressed : btnHeld;
      const int dashHoldFrames = std::max(0, cfg.actions.dash.holdFrames);
      const int dashHeldFrames = actionHeldFrames(ctl, cfg.actions.dash.input);
      const bool holdSatisfied =
          (cfg.actions.dash.trigger != ActionTrigger::Hold) || dashHeldFrames >= dashHoldFrames;
      bool allowed = true;
      if (!g.onGround) {
        if (!cfg.actions.dash.allowAir) {
          allowed = false;
        }
        if (cfg.actions.dash.airDashes >= 0 && rt.airDashesUsed >= cfg.actions.dash.airDashes) {
          allowed = false;
        }
      }

      if (trig && holdSatisfied && allowed) {
        rt.dashRemainingFrames = cfg.actions.dash.dashTimeFrames;
        acd = cfg.actions.dash.cooldownFrames;
        rt.dashDirX = signOrFacing(rawWish, rt.facingX);
        if (!g.onGround) {
          ++rt.airDashesUsed;
        }
      }
    }

    if (!controlsLocked && rt.dashRemainingFrames > 0) {
      // dash locks horizontal speed (simple)
      v.v.x = static_cast<float>(rt.dashDirX) * cfg.actions.dash.dashSpeed;
      --rt.dashRemainingFrames;
    }

    bool spinInputActive = false;
    bool spindashLaunched = false;
    int spindashLaunchDir = 0;

    // action: spindash
    if (!controlsLocked && spindashEnabled) {
      bool btnPressed = false;
      bool btnHeld = false;
      bool btnReleased = false;
      actionButton(ctl, cfg.actions.spindash.input, btnPressed, btnHeld, btnReleased);

      const int downWindowFrames = spindashDownWindowFrames;
      const int btnPressedFrames = actionPressedFrames(ctl, cfg.actions.spindash.input);
      const bool downHeld = !spindashRequireDown || ctl.downHeld;
      bool downWindowSatisfied = false;
      if (spindashRequireDown && downWindowFrames > 0) {
        const bool btnRecent = btnPressedFrames <= downWindowFrames;
        const bool downRecent = ctl.downPressedFrames <= downWindowFrames;
        downWindowSatisfied = (ctl.downHeld && btnRecent) || (btnHeld && downRecent);
      }
      const bool downSatisfied = downHeld || downWindowSatisfied;
      const int tapBoostFrames = std::max(0, cfg.actions.spindash.tapBoostFrames);
      const int chargeFrames = std::max(0, cfg.actions.spindash.chargeFrames);

      if (!g.onGround) {
        rt.spindashCharging = false;
        rt.spindashChargeFrames = 0;
      } else {
        bool startedThisFrame = false;
        const bool startOnDownPress = spindashRequireDown && downWindowFrames > 0 &&
                                      ctl.downPressed && btnHeld &&
                                      btnPressedFrames <= downWindowFrames;
        if (!rt.spindashCharging && (btnPressed || startOnDownPress) && downSatisfied) {
          rt.spindashCharging = true;
          rt.spindashChargeFrames = 0;
          rt.spinning = false;
          jbuf = 0;
          startedThisFrame = true;
        }

        if (rt.spindashCharging) {
          if (spindashRequireDown && !ctl.downHeld) {
            // Release down to launch.
            const int denom = std::max(1, chargeFrames);
            float t01 = static_cast<float>(rt.spindashChargeFrames) / static_cast<float>(denom);
            t01 = std::clamp(t01, 0.0F, 1.0F);

            const float lo = cfg.actions.spindash.minLaunchSpeed;
            const float hi = cfg.actions.spindash.maxLaunchSpeed;
            const float speed = lo + (hi - lo) * t01;

            const int dirSign = signOrFacing(wish, rt.facingX);
            v.v.x = static_cast<float>(dirSign) * speed;
            spindashLaunched = true;
            spindashLaunchDir = dirSign;

            rt.spindashCharging = false;
            rt.spindashChargeFrames = 0;
          } else {
            rt.spindashChargeFrames = std::min(rt.spindashChargeFrames + 1, chargeFrames);
            if (!startedThisFrame && btnPressed && tapBoostFrames > 0) {
              rt.spindashChargeFrames =
                  std::min(rt.spindashChargeFrames + tapBoostFrames, chargeFrames);
            }
            v.v.x = 0.0F;

            if (!spindashRequireDown && btnReleased) {
              const int denom = std::max(1, chargeFrames);
              float t01 = static_cast<float>(rt.spindashChargeFrames) / static_cast<float>(denom);
              t01 = std::clamp(t01, 0.0F, 1.0F);

              const float lo = cfg.actions.spindash.minLaunchSpeed;
              const float hi = cfg.actions.spindash.maxLaunchSpeed;
              const float speed = lo + (hi - lo) * t01;

              const int dirSign = signOrFacing(wish, rt.facingX);
              v.v.x = static_cast<float>(dirSign) * speed;
              spindashLaunched = true;
              spindashLaunchDir = dirSign;

              rt.spindashCharging = false;
              rt.spindashChargeFrames = 0;
            }
          }
        }
      }
    }

    // action: spin
    if (!controlsLocked && cfg.actions.spin.enabled) {
      bool btnPressed = false;
      bool btnHeld = false;
      bool btnReleased = false;
      actionButton(ctl, cfg.actions.spin.input, btnPressed, btnHeld, btnReleased);

      bool trig = (cfg.actions.spin.trigger == ActionTrigger::Press) ? btnPressed : btnHeld;
      // keep it minimal: when held, apply extra friction if above min speed
      if (trig && std::fabs(v.v.x) >= cfg.actions.spin.minSpeed && g.onGround) {
        spinInputActive = true;
        v.v.x = approach(v.v.x, 0.0F,
                         cfg.actions.spin.spinFriction * kSpinFrictionToDecelScale);  // px/frame
      }
    }

    if (spindashLaunched) {
      rt.spinning = true;
      rt.spinFacingX = (spindashLaunchDir != 0) ? spindashLaunchDir : rt.facingX;
      // Start rolling after spindash (Sonic-style)
      if (cfg.states.rolling.enabled && cfg.states.rolling.autoStartAfterSpindash) {
        rt.rolling = true;
      }
    }

    if (spinInputActive) {
      if (!rt.spinning) {
        rt.spinFacingX = rt.facingX;
      }
      rt.spinning = true;
    }

    if (rt.spinning) {
      static constexpr float kSpinStopSpeed = 20.0F;
      const float spinStopSpeed = std::max(kSpinStopSpeed, cfg.actions.spin.minSpeed);
      if (std::fabs(v.v.x) < spinStopSpeed && !spinInputActive && !spindashLaunched) {
        rt.spinning = false;
      }
    }
    if (!g.onGround) {
      rt.spinning = false;
    }

    // Rolling state: terminate when speed drops below threshold (Sonic-accurate: immediate)
    if (rt.rolling) {
      if (g.onGround && std::fabs(v.v.x) < cfg.states.rolling.stopSpeed && !spindashLaunched) {
        rt.rolling = false;
      }
      // Rolling persists in air (roll-jump), but terminates on landing if slow
    }

    // Hitbox management: apply reduced hitbox during rolling/sliding.
    // Both rolling and sliding can have hitbox overrides (0 = use normal).
    {
      const float normalW = cfg.collision.w;
      const float normalH = cfg.collision.h;

      // Determine target hitbox based on current state
      float targetW = normalW;
      float targetH = normalH;
      bool wantReduced = false;

      if (rt.rolling) {
        const float rollW = cfg.states.rolling.hitboxW;
        const float rollH = cfg.states.rolling.hitboxH;
        if (rollW > 0.0F && rollH > 0.0F) {
          targetW = rollW;
          targetH = rollH;
          wantReduced = true;
        }
      } else if (rt.sliding) {
        const float slideW = cfg.states.slide.hitboxW;
        const float slideH = cfg.states.slide.hitboxH;
        if (slideW > 0.0F && slideH > 0.0F) {
          targetW = slideW;
          targetH = slideH;
          wantReduced = true;
        }
      }

      // Apply hitbox change if needed
      if (wantReduced && !rt.hitboxReduced) {
        // Entering reduced hitbox state
        applyHitboxChange(box, t, targetW, targetH);
        rt.hitboxReduced = true;
        rt.hitboxFromSlide = rt.sliding;
      } else if (!wantReduced && rt.hitboxReduced) {
        // Exiting reduced hitbox state - restore normal
        bool canStand = true;
        if (rt.hitboxFromSlide && cfg.states.slide.stayLowUnderCeiling) {
          const float standY = t.pos.y + (box.h - normalH);
          const Rect standRect{t.pos.x, standY, normalW, normalH};
          if (s.overlapsSolid(standRect, true)) {
            canStand = false;
          }
        }
        if (canStand) {
          applyHitboxChange(box, t, normalW, normalH);
          rt.hitboxReduced = false;
          rt.hitboxFromSlide = false;
        }
      } else if (wantReduced && rt.hitboxReduced) {
        // Update to current state's dimensions (in case rolling→sliding transition)
        if (box.w != targetW || box.h != targetH) {
          applyHitboxChange(box, t, targetW, targetH);
        }
        rt.hitboxFromSlide = rt.sliding;
      }
    }

    // action: fly
    bool flying = false;
    if (!controlsLocked && cfg.actions.fly.enabled) {
      bool btnPressed = false;
      bool btnHeld = false;
      bool btnReleased = false;
      actionButton(ctl, cfg.actions.fly.input, btnPressed, btnHeld, btnReleased);
      allowAirJumpOverride(ctl, g, cfg.actions.fly.input, cfg.actions.fly.allowJumpOverride,
                           btnPressed, btnHeld, btnReleased);

      bool trig = (cfg.actions.fly.trigger == ActionTrigger::Press) ? btnPressed : btnHeld;
      if (trig && !g.onGround) {
        flying = true;
        if (cfg.actions.fly.upAccel > 0.0F) {
          v.v.y -= cfg.actions.fly.upAccel;  // px/frame²
        }

        if (cfg.actions.fly.maxUpSpeed > 0.0F) {
          v.v.y = std::max(v.v.y, -cfg.actions.fly.maxUpSpeed);
        }
      }
    }

    // action: glide
    bool gliding = false;
    if (!controlsLocked && cfg.actions.glide.enabled) {
      bool btnPressed = false;
      bool btnHeld = false;
      bool btnReleased = false;
      actionButton(ctl, cfg.actions.glide.input, btnPressed, btnHeld, btnReleased);
      allowAirJumpOverride(ctl, g, cfg.actions.glide.input, cfg.actions.glide.allowJumpOverride,
                           btnPressed, btnHeld, btnReleased);

      if (g.onGround) {
        rt.glideActive = false;
      } else if (cfg.actions.glide.startOnPress) {
        if (!btnHeld) {
          rt.glideActive = false;
        }
        if (btnPressed) {
          rt.glideActive = true;
        }
      } else {
        rt.glideActive = btnHeld;
      }

      gliding = rt.glideActive && !g.onGround;

      if (gliding && v.v.y > 0.0F) {
        const float m = std::clamp(cfg.actions.glide.gravityMultiplier, 0.0F, 1.0F);
        float fallGravity =
            cfg.physics.gravity * env.gravity * std::max(0.0F, cfg.jump.fallGravityMultiplier);
        v.v.y += fallGravity * (m - 1.0F);  // px/frame²
        if (cfg.actions.glide.maxFallSpeed > 0.0F) {
          v.v.y = std::min(v.v.y, cfg.actions.glide.maxFallSpeed);
        }
      }
    }

    // action: attacks (hitboxes/projectiles)
    if (!cfg.actions.attacks.empty()) {
      bool attackTriggered = false;
      for (std::size_t i = 0; i < cfg.actions.attacks.size(); ++i) {
        const AttackAction& attack = cfg.actions.attacks[i];
        if (!attack.enabled) {
          continue;
        }

        bool btnPressed = false;
        bool btnHeld = false;
        bool btnReleased = false;
        actionButton(ctl, attack.input, btnPressed, btnHeld, btnReleased);
        (void)btnReleased;

        auto& art = rt.attacks[i];
        if (!btnHeld) {
          art.heldLatch = false;
        }

        if (controlsLocked || attackTriggered || atkCd > 0) {
          continue;
        }
        if (!attack.allowAir && !g.onGround) {
          continue;
        }
        if (!hasPowerup(cfg, attack.powerupId)) {
          continue;
        }

        const bool trig = (attack.trigger == ActionTrigger::Press) ? btnPressed : btnHeld;
        if (!trig) {
          continue;
        }
        if (attack.trigger == ActionTrigger::Hold && art.heldLatch) {
          continue;
        }

        if (attack.projectile.enabled) {
          spawnAttackProjectile(w, id, attack, rt.facingX);
        } else {
          spawnAttackHitbox(w, id, attack, rt.facingX);
        }

        const int animFrames = std::max(1, attack.startupFrames + attack.activeFrames);
        rt.attackAnimFrames = animFrames;
        rt.attackAnimTotalFrames = animFrames;
        rt.attackAnimKind =
            attack.projectile.enabled ? AttackAnimKind::Shoot : AttackAnimKind::Melee;

        atkCd = std::max(0, attack.cooldownFrames);
        if (attack.trigger == ActionTrigger::Hold) {
          art.heldLatch = true;
        }
        attackTriggered = true;
      }
    }

    if (cfg.math.subpixel > 1) {
      v.v.x = util::quantizeToGrid(v.v.x, cfg.math.subpixel, cfg.math.quantize);
      v.v.y = util::quantizeToGrid(v.v.y, cfg.math.subpixel, cfg.math.quantize);
    }

    as.dashing = (rt.dashRemainingFrames > 0);
    as.dashRemainingFrames = rt.dashRemainingFrames;
    as.airDashesUsed = rt.airDashesUsed;
    as.spindashCharging = rt.spindashCharging;
    as.spindashChargeFrames = rt.spindashChargeFrames;
    as.spinning = rt.spinning;
    as.spinFacingX = rt.spinFacingX;
    as.rolling = rt.rolling;
    as.sliding = rt.sliding;
    as.slideRemainingFrames = rt.slideRemainingFrames;
    as.flying = flying;
    as.gliding = gliding;
    as.facingX = rt.facingX;
    as.walling = walling;
    as.wallDirX = wallDirX;
    as.dashHeld = dashBtnHeld;
    as.dashWallKickFrames = rt.dashWallKickFrames;
    as.attackAnimFrames = rt.attackAnimFrames;
    as.attackAnimTotalFrames = rt.attackAnimTotalFrames;
    as.attackAnimKind = rt.attackAnimKind;

    // Sync runtime timer values back to registry for debug display
    if (auto* jbufPtr = w.registry.try_get<JumpBufferFrames>(id)) {
      jbufPtr->value = rt.jumpBufferFrames;
    }
    if (auto* acdPtr = w.registry.try_get<ActionCooldownFrames>(id)) {
      acdPtr->value = rt.actionCooldownFrames;
    }
    if (auto* atkPtr = w.registry.try_get<AttackCooldownFrames>(id)) {
      atkPtr->value = rt.attackCooldownFrames;
    }
    if (auto* dropPtr = w.registry.try_get<DropThroughFrames>(id)) {
      dropPtr->value = rt.dropThroughFrames;
    }
    if (auto* lockPtr = w.registry.try_get<HurtLockoutFrames>(id)) {
      lockPtr->value = rt.hurtLockoutFrames;
    }

    // collision is handled in integrate() system (single place)
    (void)t;
    (void)box;
  }
}
