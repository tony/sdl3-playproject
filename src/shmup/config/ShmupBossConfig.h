#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace shmup {

// Boss configuration for SHMUP game mode.
// Bosses have multiple phases, weak points, and special animations.
struct ShmupBossConfig {
  int version = 0;
  std::string id;
  std::string displayName;

  // Core stats
  struct Stats {
    int totalHealth = 1000;
    int scoreValue = 10000;
    float contactDamage = 10.0F;
  } stats;

  // Collision box (main body)
  struct Collision {
    float w = 128.0F;
    float h = 96.0F;
  } collision;

  // Entrance animation configuration
  struct Entrance {
    float startX = 1400.0F;   // Off-screen right
    float startY = 360.0F;    // Vertical center
    float targetX = 1000.0F;  // Final X position
    float targetY = 360.0F;   // Final Y position
    float durationSeconds = 3.0F;
    bool invulnerableDuring = true;
  } entrance;

  // Death sequence configuration
  struct Death {
    int explosionCount = 8;
    float explosionInterval = 0.15F;
    float finalExplosionDelay = 0.5F;
  } death;

  // Movement pattern configuration (reuse enemy enums)
  enum class MovementType : std::uint8_t {
    None,
    Linear,
    Sine,
    Chase,
    Orbit,
    Figure8,
  };

  // Fire pattern configuration
  enum class FireType : std::uint8_t {
    None,
    Aimed,
    Spread,
    Circular,
  };

  // Phase-specific movement settings
  struct MovementConfig {
    MovementType type = MovementType::Sine;
    float velocityX = 0.0F;
    float velocityY = 0.0F;
    float amplitude = 80.0F;
    float frequency = 0.02F;
    bool oscillateY = true;
    float chaseSpeed = 1.0F;
    float turnRate = 0.03F;
    float boundsMinY = 100.0F;
    float boundsMaxY = 620.0F;
    float speedMultiplier = 1.0F;
  };

  // Phase-specific firing settings
  struct FireConfig {
    FireType type = FireType::Spread;
    float fireInterval = 2.0F;
    int shotCount = 5;
    float spreadAngle = 0.8F;
    float rotationSpeed = 0.0F;
    int burstCount = 1;
    float burstInterval = 0.1F;

    // Projectile configuration
    struct Projectile {
      float speed = 4.0F;
      float damage = 1.0F;
      int lifetimeFrames = 180;
      float w = 10.0F;
      float h = 10.0F;
    } projectile;
  };

  // Minion spawning configuration
  struct MinionConfig {
    bool enabled = false;
    std::string minionType;
    float spawnInterval = 5.0F;
    int maxActive = 4;
    float spawnOffsetX = -80.0F;
    float spawnOffsetY = 0.0F;
  };

  // Phase definition
  struct PhaseConfig {
    std::string id;
    float healthThreshold = 1.0F;  // Phase active when health ratio <= this
    MovementConfig movement;
    FireConfig fire;
    MinionConfig minions;
    std::vector<std::string> activeWeakPoints;
  };
  std::vector<PhaseConfig> phases;

  // Weak point definition
  struct WeakPointConfig {
    std::string id;
    float offsetX = 0.0F;
    float offsetY = 0.0F;
    float hitboxW = 24.0F;
    float hitboxH = 24.0F;
    float damageMultiplier = 2.0F;
    int health = -1;  // -1 = indestructible, follows boss health
    bool targetable = true;
  };
  std::vector<WeakPointConfig> weakPoints;

  // Rendering configuration
  struct Render {
    std::unordered_map<std::string, std::string> sheets;
    int frameW = 128;
    int frameH = 96;
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

    [[nodiscard]] std::string getSheet() const {
      if (sheets.empty())
        return {};
      auto it = sheets.find("default");
      return (it != sheets.end()) ? it->second : sheets.begin()->second;
    }
  } render;

  bool loadFromToml(const char* path);
};

// Registry for caching loaded boss configs
class ShmupBossRegistry {
 public:
  static const ShmupBossConfig* get(const std::string& id);
  static bool load(const std::string& id, const char* path);
  static void clear();

 private:
  static inline std::unordered_map<std::string, ShmupBossConfig> cache_;
};

}  // namespace shmup
