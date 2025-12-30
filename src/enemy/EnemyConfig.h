#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

struct EnemyConfig {
  int version = 0;
  std::string id;
  std::string displayName;

  enum class Type : std::uint8_t {
    Walker,
    Spiky,
    Shooter,
    Hopper,
  };

  struct Collision {
    float w = 24.0F;
    float h = 24.0F;
  } collision;

  struct Move {
    float speed = 0.6F;         // px/frame
    float gravity = 0.2F;       // px/frame^2
    float maxFallSpeed = 4.0F;  // px/frame
    bool turnOnWall = true;
    bool turnOnEdge = true;
  } move;

  struct Combat {
    int health = 1;
    float contactDamage = 1.0F;
    bool stompable = true;
    float stompBounceVy = -4.0F;  // px/frame upward (negative)
    int iframesMs = 250;
    float knockbackVx = 1.5F;   // px/frame
    float knockbackVy = -2.5F;  // px/frame
  } combat;

  struct Hopper {
    int intervalFrames = 60;
    float hopSpeedX = 0.8F;   // px/frame
    float hopSpeedY = -4.0F;  // px/frame (negative = up)
  } hopper;

  struct Projectile {
    float speed = 2.0F;        // px/frame
    float gravity = 0.0F;      // px/frame^2
    int lifetimeFrames = 120;  // frames
    float w = 8.0F;
    float h = 8.0F;
    float offsetX = 12.0F;
    float offsetY = -6.0F;
    float damage = 1.0F;
    float knockbackVx = 2.0F;   // px/frame
    float knockbackVy = -2.5F;  // px/frame
  };

  struct Shooter {
    int fireIntervalFrames = 90;
    int warmupFrames = 0;
    Projectile projectile{};
  } shooter;

  struct RenderClip {
    int row = 0;
    int start = 0;
    int frames = 1;
    float fps = 8.0F;
  };

  struct Render {
    std::unordered_map<std::string, std::string> sheets;  // direction -> path
    int frameW = 64;
    int frameH = 64;
    float scale = 2.0F;
    float offsetX = 0.0F;
    float offsetY = 38.0F;
    std::unordered_map<std::string, RenderClip> anims;

    [[nodiscard]] bool hasSprites() const { return !sheets.empty(); }

    [[nodiscard]] std::string getSheet(int facingX) const {
      if (sheets.empty())
        return {};
      const std::string key = (facingX < 0) ? "west" : "east";
      auto it = sheets.find(key);
      return (it != sheets.end()) ? it->second : sheets.begin()->second;
    }
  } render;

  Type type = Type::Walker;

  bool loadFromToml(const char* path);
};
