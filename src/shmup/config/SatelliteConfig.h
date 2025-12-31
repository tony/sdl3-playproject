#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace shmup {

struct SatelliteConfig {
  int version = 0;
  std::string id;
  std::string displayName;

  enum class PositionMode : std::uint8_t {
    Fixed,      // Static offset from player
    Orbit,      // Circular orbit around player
    Formation,  // Tightens with power level / focus
  };

  PositionMode positionMode = PositionMode::Fixed;

  struct Position {
    float fixedOffsetX = 20.0F;
    float fixedOffsetY = 0.0F;
    float orbitRadius = 30.0F;
    float orbitSpeed = 0.05F;      // Radians per frame
    float focusedRadius = 15.0F;   // Tighter orbit when focused
    float focusedOffsetX = 10.0F;  // Tighter fixed position when focused
    float focusedOffsetY = 0.0F;
  } position;

  struct Firing {
    bool inheritWeapon = true;     // Use owner's weapon
    std::string overrideWeapon;    // If not inheriting, use this weapon
    int overrideLevel = 1;         // Fixed level for override weapon
    float fireAngleOffset = 0.0F;  // Angle offset from forward (radians)
  } firing;

  struct Render {
    std::string sprite;
    int frameW = 16;
    int frameH = 16;
    int frameCount = 1;
    float fps = 0.0F;
    float scale = 1.0F;
  } render;

  // Load configuration from TOML file
  bool loadFromToml(const char* path);
};

// Registry for caching loaded satellite configs
class SatelliteRegistry {
 public:
  static const SatelliteConfig* get(const std::string& id);
  static bool load(const std::string& id, const char* path);
  static void clear();

 private:
  static inline std::unordered_map<std::string, SatelliteConfig> cache_;
};

}  // namespace shmup
