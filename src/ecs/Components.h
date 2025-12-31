#pragma once

#include <limits>
#include <string>

#include "ecs/Entity.h"

struct Vec2 {
  float x = 0.0F;
  float y = 0.0F;
  Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
  Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
  Vec2 operator*(float s) const { return {x * s, y * s}; }
};

struct Transform {
  Vec2 pos{};
};

struct Velocity {
  Vec2 v{};
};

struct AABB {
  float w = 24.0F;
  float h = 32.0F;
};

struct InputState {
  static constexpr int kUnpressedFrames = std::numeric_limits<int>::max();

  bool left = false;
  bool right = false;
  bool upPressed = false;
  bool upHeld = false;
  bool upReleased = false;
  bool downPressed = false;
  bool downHeld = false;
  bool downReleased = false;
  bool jumpPressed = false;
  bool jumpHeld = false;
  bool jumpReleased = false;
  bool action1Pressed = false;  // spin/dash/etc
  bool action1Held = false;     // spin/dash/etc
  bool action1Released = false;
  bool action2Pressed = false;  // fly/etc
  bool action2Held = false;     // fly/etc
  bool action2Released = false;

  int upHeldFrames = 0;
  int upPressedFrames = kUnpressedFrames;  // frames since press (0 = this frame)
  int downHeldFrames = 0;
  int downPressedFrames = kUnpressedFrames;
  int jumpHeldFrames = 0;
  int jumpPressedFrames = kUnpressedFrames;
  int action1HeldFrames = 0;
  int action1PressedFrames = kUnpressedFrames;
  int action2HeldFrames = 0;
  int action2PressedFrames = kUnpressedFrames;
};

struct CharacterTag {};

struct EnemyTag {};

struct EnemyState {
  int facingX = 1;
  bool hasPatrol = false;
  float patrolMinX = 0.0F;
  float patrolMaxX = 0.0F;
  int aiTimerFrames = 0;
  int attackTimerFrames = 0;
  int health = 1;
  int invincibleMs = 0;
};

struct EnemyAnimState {
  std::string currentAnim = "idle";
  int frame = 0;
  float timer = 0.0F;  // Accumulates delta time for frame advancement
};

struct CollectibleTag {};

struct CollectibleState {
  int value = 1;
  int healthRestore = 0;
  float bobTimer = 0.0F;   // For floating animation
  float animTimer = 0.0F;  // For sprite animation
  int animFrame = 0;
};

struct ProjectileTag {};

struct ProjectileState {
  float damage = 1.0F;
  float gravity = 0.0F;
  float knockbackVx = 0.0F;
  float knockbackVy = 0.0F;
  int lifetimeFrames = 0;
  int iframesMs = 250;
  bool fromEnemy = true;
};

struct AttackHitboxTag {};

struct AttackHitboxState {
  EntityId owner = kInvalidEntity;
  float damage = 1.0F;
  float knockbackVx = 0.0F;
  float knockbackVy = 0.0F;
  int startupFrames = 0;
  int activeFrames = 0;
  int hitsRemaining = 1;
  float offsetX = 0.0F;
  float offsetY = 0.0F;
  bool fromPlayer = true;
};

struct DebugName {
  std::string name;
};

struct Grounded {
  bool onGround = false;
};

struct TimerFrames {
  int value = 0;
};

// Specific timer components for EnTT (each needs a distinct type)
struct CoyoteFrames {
  int value = 0;
};
struct JumpBufferFrames {
  int value = 0;
};
struct ActionCooldownFrames {
  int value = 0;
};
struct AttackCooldownFrames {
  int value = 0;
};
struct DropThroughFrames {
  int value = 0;
};
struct HurtLockoutFrames {
  int value = 0;
};

enum class AnimStateId {
  Idle,
  Run,
  Jump,
  Fall,
  Dash,
  Spin,
  Fly,
  Glide,
  SpindashCharge,
  AttackMelee,
  AttackShoot,
};

struct AnimState {
  AnimStateId id = AnimStateId::Idle;
};

enum class AttackAnimKind {
  None,
  Melee,
  Shoot,
};

struct ActionState {
  bool dashing = false;
  int dashRemainingFrames = 0;
  int airDashesUsed = 0;
  bool spindashCharging = false;
  int spindashChargeFrames = 0;
  bool spinning = false;
  int spinFacingX = 1;
  bool rolling = false;          // Sonic-style post-spindash roll (affects physics)
  bool sliding = false;          // Mega Man-style slide
  int slideRemainingFrames = 0;  // Slide timer (frames)
  bool flying = false;
  bool gliding = false;
  bool walling = false;
  int wallDirX = 0;
  bool dashHeld = false;
  int dashWallKickFrames = 0;
  int facingX = 1;
  int attackAnimFrames = 0;
  int attackAnimTotalFrames = 0;
  AttackAnimKind attackAnimKind = AttackAnimKind::None;
};
