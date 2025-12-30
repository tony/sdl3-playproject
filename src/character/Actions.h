#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class ActionTrigger : std::uint8_t { Press, Hold };

enum class ActionInput : std::uint8_t {
  Action1,
  Action2,
  Jump,
};

struct SpinAction {
  bool enabled = false;
  ActionTrigger trigger = ActionTrigger::Hold;
  ActionInput input = ActionInput::Action1;
  float minSpeed = 0.0F;
  float spinFriction = 0.0F;
};

struct DashAction {
  static constexpr int kUnlimitedAirDashes = -1;

  bool enabled = false;
  ActionTrigger trigger = ActionTrigger::Press;
  ActionInput input = ActionInput::Action1;
  int holdFrames = 0;
  int cooldownFrames = 0;
  int dashTimeFrames = 0;
  float dashSpeed = 0.0F;  // px/frame
  bool allowAir = true;
  bool allowJumpOverride = true;        // allow jump to trigger dash while airborne
  bool preserveMomentum = false;        // keep dash speed through a dash jump
  int airDashes = kUnlimitedAirDashes;  // -1 = unlimited, 0 = none, N = N per airborne
};

struct FlyAction {
  bool enabled = false;
  ActionTrigger trigger = ActionTrigger::Hold;
  ActionInput input = ActionInput::Action2;
  float upAccel = 0.0F;
  float maxUpSpeed = 0.0F;
  bool allowJumpOverride = true;  // allow jump to trigger fly while airborne
};

struct GlideAction {
  static constexpr float kDefaultGravityMultiplier = 0.35F;
  static constexpr float kDefaultMaxFallSpeed = 4.17F;  // 500 px/s at 120Hz

  bool enabled = false;
  ActionTrigger trigger = ActionTrigger::Hold;
  ActionInput input = ActionInput::Action2;
  bool startOnPress = false;
  float gravityMultiplier = kDefaultGravityMultiplier;
  float maxFallSpeed = kDefaultMaxFallSpeed;  // px/frame
  bool allowJumpOverride = true;              // allow jump to trigger glide while airborne
};

struct SpindashAction {
  static constexpr int kDefaultChargeFrames = 60;         // 500ms at 120Hz
  static constexpr float kDefaultMinLaunchSpeed = 1.67F;  // 200 px/s at 120Hz
  static constexpr float kDefaultMaxLaunchSpeed = 7.5F;   // 900 px/s at 120Hz

  bool enabled = false;
  ActionTrigger trigger = ActionTrigger::Hold;
  ActionInput input = ActionInput::Action1;
  bool requireDown = false;
  int downWindowFrames = 0;  // allow Down + input within N frames (0 = same-frame)
  int chargeFrames = kDefaultChargeFrames;
  float minLaunchSpeed = kDefaultMinLaunchSpeed;  // px/frame
  float maxLaunchSpeed = kDefaultMaxLaunchSpeed;  // px/frame
  int tapBoostFrames = 0;
};

struct WallAction {
  static constexpr float kDefaultProbe = 1.0F;
  static constexpr float kDefaultSlideGravityMultiplier = 0.35F;
  static constexpr float kDefaultSlideMaxFallSpeed = 2.5F;  // 300 px/s at 120Hz
  static constexpr int kDefaultDetachFrames = 18;           // 150ms at 120Hz

  bool enabled = false;

  // How far to probe into a wall to consider it "touching" (px).
  float probe = kDefaultProbe;

  // While holding into a wall (and falling), apply a wall-slide profile.
  // If false, touching the wall is enough (no input required).
  bool requireInput = true;
  float slideGravityMultiplier = kDefaultSlideGravityMultiplier;  // 0..1
  float slideMaxFallSpeed = kDefaultSlideMaxFallSpeed;            // px/frame (downward)

  // Optional climb speeds while holding into a wall.
  float climbSpeed = 0.0F;    // px/frame upward (0 disables)
  float descendSpeed = 0.0F;  // px/frame downward (0 disables)

  // Optional wall-jump (jump while holding into the wall).
  float jumpImpulse = 0.0F;                 // px/frame upward (0 disables)
  float jumpVx = 0.0F;                      // px/frame away from wall (0 disables)
  int detachFrames = kDefaultDetachFrames;  // ignore wall grabs briefly after a wall jump

  // Optional dash wall-kick (dash held + jump while wall sliding).
  // X2/X3 style: dash state is checked on exact frame of jump (no buffering).
  bool dashKickEnabled = false;
  float dashKickSpeed = 0.0F;  // px/frame away from wall (0 = use dash speed)
};

struct AttackHitbox {
  float w = 18.0F;
  float h = 12.0F;
  float offsetX = 20.0F;
  float offsetY = -4.0F;
};

struct AttackProjectile {
  bool enabled = false;
  float speed = 2.4F;       // px/frame
  float gravity = 0.0F;     // px/frame^2
  int lifetimeFrames = 90;  // frames
  float w = 8.0F;
  float h = 8.0F;
  float offsetX = 12.0F;
  float offsetY = -6.0F;
  float damage = 1.0F;
  float knockbackVx = 2.0F;   // px/frame
  float knockbackVy = -2.5F;  // px/frame
};

struct AttackAction {
  bool enabled = false;
  std::string id;
  ActionTrigger trigger = ActionTrigger::Press;
  ActionInput input = ActionInput::Action1;
  bool allowAir = true;
  int startupFrames = 0;
  int activeFrames = 6;
  int cooldownFrames = 12;
  int hits = 1;
  float damage = 1.0F;
  float knockbackVx = 1.5F;   // px/frame
  float knockbackVy = -2.5F;  // px/frame
  AttackHitbox hitbox{};
  AttackProjectile projectile{};
  std::string powerupId;
};

struct Actions {
  SpinAction spin{};
  DashAction dash{};
  FlyAction fly{};
  GlideAction glide{};
  SpindashAction spindash{};
  WallAction wall{};
  std::vector<AttackAction> attacks;
};
