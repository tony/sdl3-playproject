#pragma once

#include <SDL3/SDL_render.h>

#include <cmath>
#include <vector>

#include "visual/Palette.h"

namespace Visual {

// Epsilon for floating-point comparisons in vector operations
inline constexpr float kVectorEpsilon = 0.0001F;

struct Vec2 {
  float x = 0, y = 0;

  Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
  Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
  Vec2 operator*(float s) const { return {x * s, y * s}; }
  Vec2 operator*(Vec2 o) const { return {x * o.x, y * o.y}; }
};

inline Vec2 lerp(Vec2 a, Vec2 b, float t) {
  return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}

inline float length(Vec2 v) {
  return std::sqrt(v.x * v.x + v.y * v.y);
}

inline Vec2 normalize(Vec2 v) {
  float len = length(v);
  if (len < kVectorEpsilon) {
    return {0, 0};
  }
  return {v.x / len, v.y / len};
}

// Shape rendering primitives
class ShapeRenderer {
 public:
  explicit ShapeRenderer(SDL_Renderer* r) : renderer_(r) {}

  void setColor(Color c) { SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a); }

  // Filled circle using midpoint algorithm
  void fillCircle(float cx, float cy, float radius, Color color);

  // Filled ellipse
  void fillEllipse(float cx, float cy, float rx, float ry, Color color);

  // Filled capsule (line segment with rounded ends)
  void fillCapsule(Vec2 p1, Vec2 p2, float width, Color color);

  // Filled convex polygon
  void fillPolygon(const std::vector<Vec2>& points, Color color);

  // Filled triangle
  void fillTriangle(Vec2 a, Vec2 b, Vec2 c, Color color);

  // Filled rectangle with optional corner radius
  void fillRect(float x, float y, float w, float h, Color color, float cornerRadius = 0);

  // Vertical gradient rectangle
  void fillGradientV(float x, float y, float w, float h, Color top, Color bottom);

  // Horizontal gradient rectangle
  void fillGradientH(float x, float y, float w, float h, Color left, Color right);

 private:
  SDL_Renderer* renderer_;

  // Helper: draw horizontal line
  void hline(int x1, int x2, int y, Color c);
};

}  // namespace Visual
