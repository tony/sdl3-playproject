#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace shmup {

struct WeaponConfig {
  int version = 0;
  std::string id;
  std::string displayName;

  enum class PatternType : std::uint8_t {
    Single,  // Single forward shot
    Spread,  // Multiple shots in a fan
    Laser,   // Continuous beam
    Homing,  // Heat-seeking missiles
    Wave,    // Sinusoidal pattern
  };

  PatternType pattern = PatternType::Single;

  struct LevelStats {
    int level = 1;
    int projectileCount = 1;   // Number of projectiles per shot
    float spreadAngle = 0.0F;  // Total spread angle (radians)
    int cooldownFrames = 8;    // Frames between shots
    float damage = 1.0F;
    int pierce = 1;                // -1 = infinite (laser)
    float projectileSpeed = 8.0F;  // px/frame
  };

  std::vector<LevelStats> levels;  // Index 0 = level 1, etc.

  struct Projectile {
    float w = 8.0F;
    float h = 8.0F;
    int lifetimeFrames = 90;
    float gravity = 0.0F;  // For arc shots
    std::string sprite;

    // Homing-specific
    float homingTurnRate = 0.08F;
    float homingSeekRadius = 150.0F;
    int homingDelayFrames = 15;  // Frames before homing activates
  } projectile;

  struct Laser {
    float width = 6.0F;
    float maxLength = 400.0F;
    float extensionRate = 20.0F;  // px/frame beam growth
    float damagePerFrame = 0.5F;
  } laser;

  struct Audio {
    std::string fireSound;
    std::string hitSound;
  } audio;

  // Load configuration from TOML file
  bool loadFromToml(const char* path);

  // Get stats for a specific level (clamped to valid range)
  [[nodiscard]] const LevelStats& getLevel(int level) const;
};

// Registry for caching loaded weapon configs
class WeaponRegistry {
 public:
  static const WeaponConfig* get(const std::string& id);
  static bool load(const std::string& id, const char* path);
  static void clear();

 private:
  static inline std::unordered_map<std::string, WeaponConfig> cache_;
};

}  // namespace shmup
