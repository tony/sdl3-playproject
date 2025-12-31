#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace shmup {

// Enemy configuration for SHMUP game mode.
// Designed for pattern-based enemies typical of horizontal shooters.
struct ShmupEnemyConfig {
  int version = 0;
  std::string id;
  std::string displayName;

  // Core stats
  struct Stats {
    int health = 1;
    int scoreValue = 100;
    float contactDamage = 1.0F;
    bool invulnerable = false;
  } stats;

  // Collision box
  struct Collision {
    float w = 24.0F;
    float h = 16.0F;
  } collision;

  // Movement pattern configuration
  enum class MovementType : std::uint8_t {
    None,       // Stationary
    Linear,     // Constant velocity
    Sine,       // Oscillating (sine wave)
    Chase,      // Track toward player
    Orbit,      // Circle around a point
    Formation,  // Follow formation leader
  };

  struct Movement {
    MovementType type = MovementType::Linear;
    float velocityX = -2.0F;  // Horizontal speed (negative = left)
    float velocityY = 0.0F;   // Vertical speed

    // Sine wave parameters
    float amplitude = 30.0F;  // Oscillation amplitude (pixels)
    float frequency = 0.05F;  // Radians per frame
    bool oscillateY = true;   // true = vertical oscillation

    // Chase parameters
    float chaseSpeed = 1.5F;  // Speed when chasing
    float turnRate = 0.05F;   // Radians per frame (max turn speed)

    // Orbit parameters
    float orbitRadius = 60.0F;
    float orbitSpeed = 0.03F;  // Radians per frame
  } movement;

  // Fire pattern configuration
  enum class FireType : std::uint8_t {
    None,      // Does not fire
    Aimed,     // Fire toward player
    Spread,    // Fixed spread pattern
    Circular,  // Rotating pattern
  };

  struct Fire {
    FireType type = FireType::None;
    float fireInterval = 2.0F;  // Seconds between shots
    int warmupFrames = 0;       // Delay before first shot

    // Spread/circular parameters
    int shotCount = 1;         // Number of projectiles per shot
    float spreadAngle = 0.0F;  // Spread in radians

    // Circular parameters
    float rotationSpeed = 0.0F;  // Rotation speed for circular pattern

    // Projectile configuration
    struct Projectile {
      float speed = 3.0F;
      float damage = 1.0F;
      int lifetimeFrames = 180;
      float w = 8.0F;
      float h = 8.0F;
      float gravity = 0.0F;
    } projectile;
  } fire;

  // Rendering
  struct Render {
    std::unordered_map<std::string, std::string> sheets;  // direction -> path
    int frameW = 32;
    int frameH = 32;
    float scale = 1.0F;
    float offsetX = 0.0F;
    float offsetY = 0.0F;

    struct AnimClip {
      int row = 0;
      int start = 0;
      int frames = 1;
      float fps = 8.0F;
    };
    std::unordered_map<std::string, AnimClip> anims;

    [[nodiscard]] bool hasSprites() const { return !sheets.empty(); }

    [[nodiscard]] std::string getSheet(int facingX) const {
      if (sheets.empty())
        return {};
      const std::string key = (facingX < 0) ? "west" : "east";
      auto it = sheets.find(key);
      return (it != sheets.end()) ? it->second : sheets.begin()->second;
    }
  } render;

  bool loadFromToml(const char* path);
};

// Registry for caching loaded enemy configs
class ShmupEnemyRegistry {
 public:
  static const ShmupEnemyConfig* get(const std::string& id);
  static bool load(const std::string& id, const char* path);
  static void clear();

 private:
  static inline std::unordered_map<std::string, ShmupEnemyConfig> cache_;
};

}  // namespace shmup
