#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace shmup {

// Level configuration for SHMUP stages.
// Defines waves, backgrounds, and level properties.
struct LevelConfig {
  int version = 0;
  std::string id;
  std::string displayName;

  // Level properties
  struct Properties {
    float scrollSpeed = 60.0F;      // Pixels per second
    float duration = 180.0F;        // Level duration in seconds
    float bossPosition = 10000.0F;  // Scroll position for boss trigger
  } properties;

  // Background layer definition
  struct Background {
    std::string spritePath;
    float scrollSpeedRatio = 1.0F;  // Relative to main scroll (parallax)
    bool repeatX = true;
    bool repeatY = false;
    float offsetY = 0.0F;
    int layer = 0;  // Lower = further back
  };
  std::vector<Background> backgrounds;

  // Wave spawn definition
  enum class Formation : std::uint8_t {
    Single,    // Single enemy
    Line,      // Horizontal line
    Column,    // Vertical line
    VShape,    // V formation
    InverseV,  // Inverted V formation
    Diagonal,  // Diagonal line
    Random,    // Random positions
  };

  struct Wave {
    std::string id;
    std::string enemyType;  // Enemy config ID to spawn
    Formation formation = Formation::Single;
    int count = 1;                 // Number of enemies
    float triggerPosition = 0.0F;  // Scroll X position to trigger spawn
    float spawnX = 1300.0F;        // Spawn X position (off right side)
    float spawnY = 360.0F;         // Spawn Y position (center)
    float spacing = 48.0F;         // Spacing between enemies
    float delayBetween = 0.0F;     // Delay in seconds between spawns
  };
  std::vector<Wave> waves;

  // Boss section definition
  struct BossSection {
    std::string bossId;
    float triggerPosition = 10000.0F;
    bool pauseScroll = true;
  };
  std::vector<BossSection> bossSections;

  bool loadFromToml(const char* path);
};

// Registry for caching loaded level configs
class LevelRegistry {
 public:
  static const LevelConfig* get(const std::string& id);
  static bool load(const std::string& id, const char* path);
  static void clear();

 private:
  static inline std::unordered_map<std::string, LevelConfig> cache_;
};

}  // namespace shmup
