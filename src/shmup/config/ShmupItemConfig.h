#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace shmup {

// Configuration for power-up items in SHMUP mode.
// Items are collectible pickups that grant effects when touched.
struct ShmupItemConfig {
  int version = 0;
  std::string id;
  std::string displayName;

  // Effect type enumeration
  enum class EffectType : std::uint8_t {
    WeaponUpgrade,  // Increase weapon level
    Life,           // Grant extra life
    Bomb,           // Grant bomb stock
    ScoreBonus,     // Add to score
    Shield,         // Grant temporary invincibility
    FullPower,      // Max out weapon level instantly
  };

  // Effect configuration
  struct Effect {
    EffectType type = EffectType::ScoreBonus;
    int value = 100;  // Effect magnitude (score amount, levels, frames, etc.)
  } effect;

  // Pickup properties
  struct Pickup {
    float radius = 24.0F;        // Collision radius for collection
    int lifetimeFrames = 600;    // Frames before despawn (10s @ 60fps)
    float magnetRange = 100.0F;  // Range to start magnetizing toward player
    float magnetSpeed = 6.0F;    // Speed when being pulled to player
  } pickup;

  // Movement type enumeration
  enum class MovementType : std::uint8_t {
    Float,       // Drift downward slowly
    Bounce,      // Bounce off screen edges
    Magnet,      // Always move toward player
    Stationary,  // Stay in place
  };

  // Movement configuration
  struct Movement {
    MovementType type = MovementType::Float;
    float velocityX = 0.0F;
    float velocityY = 0.5F;
    float bounceSpeed = 2.0F;  // Speed after bouncing
  } movement;

  // Rendering configuration (placeholder for sprites)
  struct Render {
    std::string sprite;
    int frameW = 16;
    int frameH = 16;
    int frames = 1;
    float fps = 8.0F;
    float scale = 2.0F;
  } render;

  bool loadFromToml(const char* path);
};

// Registry for caching loaded item configs
class ShmupItemRegistry {
 public:
  static const ShmupItemConfig* get(const std::string& id);
  static bool load(const std::string& id, const char* path);
  static void clear();

 private:
  static inline std::unordered_map<std::string, ShmupItemConfig> cache_;
};

}  // namespace shmup
