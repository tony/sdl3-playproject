#pragma once

#include <cstdint>
#include <string>
#include <vector>

#ifdef SANDBOX_HAS_ENTT
#include <entt/entt.hpp>
#endif

namespace shmup {

// Entity handle type (uses EnTT when available, otherwise a simple alias)
#ifdef SANDBOX_HAS_ENTT
using EntityId = entt::entity;
inline constexpr EntityId kInvalidEntity = entt::null;
#else
using EntityId = std::uint32_t;
inline constexpr EntityId kInvalidEntity = static_cast<EntityId>(-1);
#endif

// =============================================================================
// Core Components (shared with other systems)
// =============================================================================

struct Vec2 {
  float x = 0.0F;
  float y = 0.0F;

  Vec2 operator+(const Vec2& rhs) const { return {x + rhs.x, y + rhs.y}; }
  Vec2 operator-(const Vec2& rhs) const { return {x - rhs.x, y - rhs.y}; }
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
  float h = 24.0F;
};

// =============================================================================
// Tag Components
// =============================================================================

struct ShmupPlayerTag {};
struct ShmupEnemyTag {};
struct ShmupProjectileTag {};
struct ShmupSatelliteTag {};
struct ShmupBossTag {};
struct ScrollsWithWorld {};  // Entity scrolls left with the world
struct Stationary {};        // Entity stays fixed (turrets, etc.)

// =============================================================================
// Player Ship State
// =============================================================================

struct ShipState {
  std::string shipId;
  int health = 3;
  int lives = 3;
  int invincibleFrames = 0;
  bool focused = false;  // Slow mode for precise movement
  int facingX = 1;       // -1 or +1 for sprite direction
};

// =============================================================================
// Weapon System
// =============================================================================

struct WeaponMount {
  std::string weaponId;
  int level = 1;           // 1-4 (MAX)
  int cooldownFrames = 0;  // Remaining frames until can fire
  float offsetX = 0.0F;    // Mount position relative to ship center
  float offsetY = 0.0F;
  int ammo = -1;  // -1 = infinite
};

struct WeaponState {
  std::vector<WeaponMount> mounts;
  int activeSlot = 0;
  bool firing = false;
};

// =============================================================================
// Satellite/Option State (Super EDF style)
// =============================================================================

struct SatelliteState {
  std::string configId;
  EntityId owner = kInvalidEntity;
  int slotIndex = 0;
  float orbitAngle = 0.0F;  // Current angle for orbit mode (radians)
  float offsetX = 0.0F;     // Current computed offset from owner
  float offsetY = 0.0F;
};

// =============================================================================
// Projectile State
// =============================================================================

struct ShmupProjectileState {
  float damage = 1.0F;
  int lifetimeFrames = 120;
  int ageFrames = 0;
  int pierceRemaining = 1;  // -1 = infinite (laser)
  bool fromPlayer = true;
  EntityId owner = kInvalidEntity;
};

struct HomingState {
  float turnRate = 0.1F;      // Radians per frame
  float seekRadius = 200.0F;  // Max distance to acquire target
  int delayFrames = 0;        // Frames before homing activates
  EntityId target = kInvalidEntity;
};

struct LaserBeamState {
  float length = 0.0F;  // Current beam length (extends over frames)
  float maxLength = 400.0F;
  float width = 8.0F;
  float damagePerFrame = 0.1F;
  bool active = false;
};

// =============================================================================
// Enemy State
// =============================================================================

struct EnemyState {
  std::string typeId;
  int health = 1;
  int maxHealth = 1;
  int scoreValue = 100;
  int damageOnContact = 10;
  bool invulnerable = false;
};

// =============================================================================
// Movement Patterns
// =============================================================================

enum class MovementType : std::uint8_t { None, Linear, Sine, Chase, Orbit, Formation, Compound };

struct Movement {
  MovementType type = MovementType::None;
  std::string patternId;

  // Linear/base velocity
  float velocityX = 0.0F;
  float velocityY = 0.0F;

  // Sine wave parameters
  float amplitude = 0.0F;
  float frequency = 0.0F;
  float phase = 0.0F;      // Current phase (radians)
  bool oscillateY = true;  // Oscillate on Y axis if true, X if false

  // Chase parameters
  float chaseSpeed = 0.0F;
  float turnRate = 0.0F;      // Radians per second
  float currentAngle = 0.0F;  // Current heading

  // Orbit parameters
  float orbitRadius = 0.0F;
  float orbitAngle = 0.0F;  // Current orbit position
  float orbitSpeed = 0.0F;  // Radians per second
  EntityId orbitCenter = kInvalidEntity;

  // Bounds (for boss movement)
  float boundsMinX = -1000.0F;
  float boundsMaxX = 2000.0F;
  float boundsMinY = 0.0F;
  float boundsMaxY = 720.0F;

  // Formation tracking
  EntityId formationLeader = kInvalidEntity;
  float formationOffsetX = 0.0F;
  float formationOffsetY = 0.0F;
};

// =============================================================================
// Fire Patterns
// =============================================================================

enum class FireType : std::uint8_t { None, Aimed, Spread, Circular, Compound };

struct Firing {
  FireType type = FireType::None;
  std::string patternId;
  std::string projectileId;

  float fireInterval = 1.0F;   // Seconds between fire events
  float timeSinceFire = 0.0F;  // Accumulator
  float initialDelay = 0.0F;   // Delay before first shot

  // Burst fire
  int shotsPerBurst = 1;
  int shotsRemaining = 0;
  float burstInterval = 0.1F;
  float burstTimer = 0.0F;

  // Spread fire
  int shotCount = 1;
  float spreadAngle = 0.0F;  // Degrees

  // Circular fire
  float rotationAngle = 0.0F;  // Current pattern rotation
  float rotationSpeed = 0.0F;  // Radians per second

  // Aim direction (computed each frame for aimed patterns)
  float aimAngle = 0.0F;
};

// =============================================================================
// Scroll State (singleton-ish, one per level)
// =============================================================================

struct ScrollState {
  float worldX = 0.0F;
  float scrollSpeed = 60.0F;  // Pixels per second
  bool paused = false;
  float levelLength = 10000.0F;
};

// =============================================================================
// Boss Components
// =============================================================================

struct BossPhase {
  std::string id;
  float healthThreshold = 1.0F;  // Phase active above this ratio
  std::string firePattern;
  std::string movementPattern;
  std::vector<std::string> weakPoints;
  bool spawnMinions = false;
  std::string minionType;
  float minionInterval = 5.0F;
  float speedMultiplier = 1.0F;
};

struct WeakPoint {
  std::string id;
  float offsetX = 0.0F;
  float offsetY = 0.0F;
  float hitboxW = 20.0F;
  float hitboxH = 20.0F;
  float damageMultiplier = 1.0F;
  int health = -1;  // -1 = indestructible
  bool destroyed = false;
};

struct BossState {
  std::string bossId;
  int totalHealth = 1000;
  int currentHealth = 1000;

  std::vector<BossPhase> phases;
  int currentPhase = 0;

  std::vector<WeakPoint> weakPoints;

  // Entrance animation
  bool entering = true;
  float entranceProgress = 0.0F;
  float entranceDuration = 3.0F;
  float targetX = 900.0F;
  float targetY = 360.0F;

  // Minion spawning
  float minionTimer = 0.0F;

  // Death sequence
  bool dying = false;
  int explosionsRemaining = 0;
  float explosionTimer = 0.0F;
};

// =============================================================================
// Player Input (normalized from SDL)
// =============================================================================

struct ShmupInput {
  float moveX = 0.0F;  // -1 to 1
  float moveY = 0.0F;  // -1 to 1
  bool fireHeld = false;
  bool firePressed = false;
  bool focusHeld = false;
  bool bombPressed = false;
};

// =============================================================================
// Sprite Rendering
// =============================================================================

struct ShmupSprite {
  std::string texturePath;
  int frame = 0;
  int frameCount = 1;
  float frameTimer = 0.0F;
  float frameRate = 8.0F;  // FPS
  int frameW = 32;
  int frameH = 32;
  float scale = 1.0F;
  float offsetX = 0.0F;
  float offsetY = 0.0F;
  bool flipX = false;
};

// =============================================================================
// Cleanup Tags
// =============================================================================

struct Despawn {};           // Mark entity for removal
struct OffScreenDespawn {};  // Auto-despawn when off-screen

// =============================================================================
// Session/Score State
// =============================================================================

struct SessionState {
  std::int64_t score = 0;
  int lives = 3;
  int bombs = 3;
  int continues = 0;
  int stageNumber = 1;
  int checkpointId = 0;
};

}  // namespace shmup
