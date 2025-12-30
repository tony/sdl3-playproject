#pragma once

#include <cstdint>
#include <string>

struct CollectibleConfig {
  int version = 0;
  std::string id;
  std::string displayName;

  enum class Type : std::uint8_t {
    Coin,
    Gem,
    Health,
    Powerup,
  };

  struct Collision {
    float w = 16.0F;
    float h = 16.0F;
  } collision;

  struct Value {
    int score = 1;
    int health = 0;
  } value;

  struct Render {
    std::string sprite;
    int frameW = 32;
    int frameH = 32;
    int frames = 1;
    float fps = 8.0F;
    float scale = 2.0F;
    float offsetX = 0.0F;
    float offsetY = 0.0F;

    [[nodiscard]] bool hasSprite() const { return !sprite.empty(); }
  } render;

  Type type = Type::Coin;

  bool loadFromToml(const char* path);
};
