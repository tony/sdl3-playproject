#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace shmup {

struct ShipConfig {
  int version = 0;
  std::string id;
  std::string displayName;

  struct Physics {
    float speed = 4.0F;           // px/frame normal movement
    float focusedSpeed = 2.0F;    // px/frame when focused
    float accel = 0.5F;           // px/frame^2 (for smooth movement)
    bool instantMovement = true;  // true = set velocity, false = accelerate
  } physics;

  struct Collision {
    float w = 6.0F;        // Hitbox width (tiny for SHMUP tradition)
    float h = 6.0F;        // Hitbox height
    float grazeW = 24.0F;  // Graze hitbox (optional scoring)
    float grazeH = 24.0F;
  } collision;

  struct WeaponSlot {
    float offsetX = 0.0F;
    float offsetY = 0.0F;
    std::string defaultWeapon;
    int defaultLevel = 1;
  };

  struct SatelliteSlot {
    float baseOffsetX = 20.0F;
    float baseOffsetY = 0.0F;
    float orbitRadius = 30.0F;
    float orbitSpeed = 0.05F;  // Radians per frame
    float focusedOffsetX = 10.0F;
    float focusedOffsetY = 0.0F;
    std::string defaultSatellite;
  };

  std::vector<WeaponSlot> weaponSlots;
  std::vector<SatelliteSlot> satelliteSlots;

  struct Render {
    std::unordered_map<std::string, std::string> sheets;  // direction -> path
    int frameW = 48;
    int frameH = 48;
    float scale = 1.0F;
    float offsetX = 0.0F;
    float offsetY = 0.0F;

    struct AnimClip {
      int row = 0;
      int start = 0;
      int frames = 1;
      float fps = 0.0F;
    };
    std::unordered_map<std::string, AnimClip> anims;
  } render;

  // Load configuration from TOML file (with include support)
  bool loadFromToml(const char* path);
};

// Registry for caching loaded ship configs
class ShipRegistry {
 public:
  static const ShipConfig* get(const std::string& id);
  static bool load(const std::string& id, const char* path);
  static void clear();

 private:
  static inline std::unordered_map<std::string, ShipConfig> cache_;
};

}  // namespace shmup
