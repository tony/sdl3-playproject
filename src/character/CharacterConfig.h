#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "character/Actions.h"

struct CharacterConfig {
  int version = 0;
  std::string id;
  std::string displayName;

  enum class JumpModel : std::uint8_t {
    Impulse,      // current behavior: v.y cut on early release
    DualGravity,  // Mario-ish: reduced rise gravity while held
    ClampVy,      // Sonic-ish: clamp upward v.y on release
    Fixed,        // MegaMan-ish: ignore early release (commitment jump)
  };

  enum class MoveModel : std::uint8_t {
    Approach,  // current behavior: v.x approaches a target via accel/decel
    Instant,   // “set velocity” style (MegaMan-ish responsiveness)
  };

  enum class CollisionSnap : std::uint8_t {
    Subpixel,  // collision uses the same subpixel grid as position
    Pixel,     // collision uses pixel-truncated positions
  };

  struct RenderClip {
    int row = 0;
    int start = 0;
    int frames = 1;
    float fps = 0.0F;
    float rotateDeg = 0.0F;
  };

  struct Render {
    std::string sheet;  // Single sheet (uses flip for opposite direction)
    std::unordered_map<std::string, std::string> sheets;  // Direction -> sheet path
    std::string icon;  // Optional icon image for UI (e.g., character selection)
    int frameW = 0;
    int frameH = 0;
    float scale = 1.0F;
    float offsetX = 0.0F;
    float offsetY = 0.0F;
    bool preferForm = false;  // If true, use form rendering even if sprites configured
    std::unordered_map<std::string, RenderClip> anims;

    // Get sheet path for facing direction (-1=left/west, +1=right/east)
    // Returns empty string if no sheet configured
    [[nodiscard]] const std::string& getSheetForFacing(int facingX) const {
      if (!sheets.empty()) {
        const char* dir = (facingX < 0) ? "west" : "east";
        if (auto it = sheets.find(dir); it != sheets.end()) {
          return it->second;
        }
      }
      return sheet;  // Fall back to single sheet (uses flip)
    }

    // True if using directional sheets (no flip needed)
    [[nodiscard]] bool hasDirectionalSheets() const { return !sheets.empty(); }

    // True if sprite rendering should be used
    [[nodiscard]] bool useSpriteRendering() const {
      return !preferForm && (hasDirectionalSheets() || !sheet.empty());
    }
  } render;

  struct FormStyle {
    struct AnchorDelta {
      float x = 0.0F;
      float y = 0.0F;
    };

    std::string id = "puppet";
    std::string paletteBase;
    std::string paletteAccent;
    std::unordered_map<std::string, std::string> colors;
    std::vector<std::string> variants;
    std::unordered_map<std::string, AnchorDelta> anchorDeltas;
  } form;

  std::vector<std::string> powerups;

  struct Physics {
    static constexpr float kDefaultGravity = 2400.0F;
    static constexpr float kDefaultMaxFallSpeed = 2200.0F;
    static constexpr float kDefaultGroundFriction = 12.0F;
    static constexpr float kDefaultAirDrag = 2.0F;

    float gravity = kDefaultGravity;
    float maxFallSpeed = kDefaultMaxFallSpeed;
    float groundFriction = kDefaultGroundFriction;
    float airDrag = kDefaultAirDrag;
  } physics;

  struct EnvironmentMultipliers {
    float gravity = 1.0F;
    float maxFallSpeed = 1.0F;
    float accel = 1.0F;
    float maxSpeed = 1.0F;
    float friction = 1.0F;
    float groundFriction = 1.0F;
    float airDrag = 1.0F;
    float turnResistance = 1.0F;
    float jumpImpulse = 1.0F;
  };

  struct Environment {
    EnvironmentMultipliers water{};
    EnvironmentMultipliers ice{};
  } environment;

  struct Move {
    static constexpr float kDefaultAccelGround = 2400.0F;
    static constexpr float kDefaultAccelAir = 1400.0F;
    static constexpr float kDefaultDecelGround = 3200.0F;
    static constexpr float kDefaultMaxSpeedGround = 800.0F;
    static constexpr float kDefaultMaxSpeedAir = 650.0F;
    static constexpr float kDefaultTurnResistance = 0.2F;

    MoveModel model = MoveModel::Approach;
    bool airControl = true;
    float accelGround = kDefaultAccelGround;
    float accelAir = kDefaultAccelAir;
    float decelGround = kDefaultDecelGround;
    float maxSpeedGround = kDefaultMaxSpeedGround;
    float maxSpeedAir = kDefaultMaxSpeedAir;
    float turnResistance = kDefaultTurnResistance;  // 0..1 (higher = more “Sonic-like inertia”)
    float turnResistanceAir = kDefaultTurnResistance;  // 0..1 (air turn damping)
  } move;

  struct Math {
    enum class QuantizeMode : std::uint8_t {
      Round,  // current behavior
      Trunc,  // fixed-point-ish (truncate towards 0)
    };

    int subpixel = 0;  // 0 or 1 disables subpixel quantization
    CollisionSnap collisionSnap = CollisionSnap::Subpixel;
    QuantizeMode quantize = QuantizeMode::Round;
  } math;

  struct Jump {
    static constexpr float kDefaultImpulse = 7.5F;       // px/frame at 120Hz (was 900 px/s)
    static constexpr int kDefaultCoyoteFrames = 10;      // 80ms at 120Hz
    static constexpr int kDefaultJumpBufferFrames = 11;  // 90ms at 120Hz
    static constexpr float kDefaultVariableCutMultiplier = 0.45F;

    struct ImpulseBySpeed {
      float minAbsVx = 0.0F;  // px/frame (absolute value)
      float impulse = 0.0F;   // px/frame (vertical jump impulse)
    };

    bool enabled = true;
    JumpModel model = JumpModel::Impulse;
    float impulse = kDefaultImpulse;
    int coyoteFrames = kDefaultCoyoteFrames;
    int jumpBufferFrames = kDefaultJumpBufferFrames;
    bool variableJump = true;
    float variableCutMultiplier = kDefaultVariableCutMultiplier;  // when releasing jump early
    float fallGravityMultiplier = 1.0F;  // gravity multiplier while falling (v.y > 0)
    int maxHoldFrames = 0;               // 0 = unlimited

    std::vector<ImpulseBySpeed> impulseBySpeed;

    // dual gravity (rise phase): gravity multipliers while v.y < 0.
    float riseGravityMultiplierHeld = 1.0F;      // while jump held (if variable_jump)
    float riseGravityMultiplierReleased = 1.0F;  // while jump released (if variable_jump)

    // clamp-vy: on early release, clamp upward velocity to this magnitude (px/frame).
    float releaseClampVy = 0.0F;

    // Optional: on jump release after a delay, apply a downward velocity kick (px/frame).
    int releaseDropAfterFrames = 0;
    float releaseDrop = 0.0F;
  } jump;

  struct Collision {
    static constexpr float kDefaultWidth = 24.0F;
    static constexpr float kDefaultHeight = 32.0F;

    float w = kDefaultWidth;
    float h = kDefaultHeight;
  } collision;

  struct States {
    struct RollingState {
      bool enabled = false;
      bool autoStartAfterSpindash = false;

      // Hitbox overrides (0 = use default collision dimensions)
      float hitboxW = 0.0F;
      float hitboxH = 0.0F;

      // Physics
      float accelMultiplier = 1.0F;
      float maxSpeedMultiplier = 1.0F;
      float frictionMultiplier = 0.5F;  // Half of normal (Sonic-accurate)
      float stopSpeed = 30.0F;          // Exit rolling below this speed (px/s)

      // Control restrictions
      bool disableAirControl = true;  // No air control after roll-jump
      bool allowJump = true;          // Can jump out of roll
    } rolling;

    struct DropThroughState {
      static constexpr int kDefaultHoldFrames = 22;  // 180ms at 120Hz

      int holdFrames = kDefaultHoldFrames;  // frames to ignore one-way collision
    } dropThrough;

    struct SlideState {
      bool enabled = false;

      // Duration
      int durationFrames = 60;      // 500ms at 120Hz
      int jumpLockoutFrames = 16;   // 133ms at 120Hz
      bool allowJumpCancel = true;  // false for MM6-style (no cancel)
      int inputWindowFrames = 0;    // allow Down+Jump within N frames (0 = same-frame)

      // Hitbox overrides (0 = use default collision dimensions)
      float hitboxW = 0.0F;
      float hitboxH = 0.0F;

      // Speed (constant, no friction decay)
      float slideSpeed = 150.0F;  // ~2.5 px/frame

      // Behavior
      bool cancelOnGroundLoss = true;   // MM-accurate: cancel if ground disappears
      bool stayLowUnderCeiling = true;  // Stay crouched if no headroom
    } slide;
  } states;

  Actions actions;

  bool loadFromToml(const char* path);
};
