#pragma once

#include <unordered_map>
#include <vector>

#include "character/CharacterConfig.h"
#include "ecs/Components.h"
#include "ecs/Entity.h"

class Stage;
class World;
struct TimeStep;

class CharacterController {
 public:
  static void attach(EntityId e, CharacterConfig cfg);
  static void detach(EntityId e);
  static void resetRuntime(EntityId e);
  static void setFacing(EntityId e, int facingX);
  static bool replaceConfig(EntityId e, CharacterConfig cfg, bool resetRuntime);
  static const CharacterConfig* get(EntityId e);

  static void tick(World& w, Stage& s, TimeStep ts);

 private:
  struct Runtime {
    struct AttackRuntime {
      bool heldLatch = false;
    };

    CharacterConfig cfg;

    // Timer state moved from ECS to runtime (avoids per-entity timer components)
    int jumpBufferFrames = 0;
    int actionCooldownFrames = 0;
    int attackCooldownFrames = 0;
    int dropThroughFrames = 0;
    int hurtLockoutFrames = 0;

    int dashRemainingFrames = 0;
    int dashDirX = 1;
    bool dashCarry = false;
    float dashCarrySpeed = 0.0F;
    bool jumpCutApplied = false;
    int jumpHeldFrames = 0;
    bool jumpReleaseDropApplied = false;
    int airDashesUsed = 0;
    bool spindashCharging = false;
    int spindashChargeFrames = 0;
    bool spinning = false;
    int spinFacingX = 1;
    bool rolling = false;               // Sonic-style rolling (post-spindash)
    bool rollJumpNoAirControl = false;  // True if jumped from roll with air control disabled
    bool sliding = false;               // Mega Man-style slide
    int slideRemainingFrames = 0;
    int slideJumpLockoutFrames = 0;
    int slideDirX = 1;
    bool hitboxReduced = false;  // True when using reduced hitbox (rolling/sliding)
    bool hitboxFromSlide = false;
    bool glideActive = false;
    int wallDetachFrames = 0;
    int dashWallKickFrames = 0;
    int facingX = 1;
    int attackAnimFrames = 0;
    int attackAnimTotalFrames = 0;
    AttackAnimKind attackAnimKind = AttackAnimKind::None;
    std::vector<AttackRuntime> attacks;
  };

  static inline std::unordered_map<EntityId, Runtime> rt_;
};
