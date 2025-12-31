#include "visual/Shapes.h"

#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_render.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace Visual {

void ShapeRenderer::hline(int x1, int x2, int y, Color c) {
  if (x1 > x2) {
    std::swap(x1, x2);
  }
  setColor(c);
  SDL_RenderLine(renderer_, static_cast<float>(x1), static_cast<float>(y), static_cast<float>(x2),
                 static_cast<float>(y));
}

void ShapeRenderer::fillCircle(float cx, float cy, float radius, Color color) {
  fillEllipse(cx, cy, radius, radius, color);
}

void ShapeRenderer::fillEllipse(float cx, float cy, float rx, float ry, Color color) {
  // Minimum radius for rendering (half pixel)
  constexpr float kMinRadius = 0.5F;
  if (rx < kMinRadius || ry < kMinRadius) {
    return;
  }

  setColor(color);

  // Midpoint ellipse algorithm - fill horizontal spans
  int iry = static_cast<int>(std::lround(ry));
  int icx = static_cast<int>(std::lround(cx));
  int icy = static_cast<int>(std::lround(cy));

  // For each scanline from top to bottom
  for (int y = -iry; y <= iry; ++y) {
    // Calculate x extent at this y using ellipse equation
    // (x/rx)^2 + (y/ry)^2 = 1
    // x = rx * sqrt(1 - (y/ry)^2)
    float yNorm = static_cast<float>(y) / static_cast<float>(iry);
    float xExtent = rx * std::sqrt(std::max(0.0F, 1.0F - yNorm * yNorm));
    int ix = static_cast<int>(std::lround(xExtent));

    SDL_RenderLine(renderer_, static_cast<float>(icx - ix), static_cast<float>(icy + y),
                   static_cast<float>(icx + ix), static_cast<float>(icy + y));
  }
}

void ShapeRenderer::fillCapsule(Vec2 p1, Vec2 p2, float width, Color color) {
  setColor(color);

  constexpr float kCapsuleRadiusScale = 0.5F;
  constexpr float kCapsuleMinLength = 0.1F;

  float radius = width * kCapsuleRadiusScale;
  Vec2 dir = p2 - p1;
  float len = length(dir);

  if (len < kCapsuleMinLength) {
    // Degenerate to circle
    fillCircle(p1.x, p1.y, radius, color);
    return;
  }

  // Perpendicular direction
  Vec2 perp = {-dir.y / len, dir.x / len};

  // Draw the rectangular body
  Vec2 c1 = p1 + perp * radius;
  Vec2 c2 = p1 - perp * radius;
  Vec2 c3 = p2 - perp * radius;
  Vec2 c4 = p2 + perp * radius;

  fillPolygon({c1, c2, c3, c4}, color);

  // Draw end caps as semicircles
  fillCircle(p1.x, p1.y, radius, color);
  fillCircle(p2.x, p2.y, radius, color);
}

void ShapeRenderer::fillTriangle(Vec2 a, Vec2 b, Vec2 c, Color color) {
  fillPolygon({a, b, c}, color);
}

void ShapeRenderer::fillPolygon(const std::vector<Vec2>& points, Color color) {
  if (points.size() < 3) {
    return;
  }

  setColor(color);

  // Find bounding box
  float minY = points[0].y;
  float maxY = points[0].y;
  for (const auto& p : points) {
    minY = std::min(minY, p.y);
    maxY = std::max(maxY, p.y);
  }

  // Scanline fill
  for (int y = static_cast<int>(minY); y <= static_cast<int>(maxY); ++y) {
    std::vector<float> intersections;

    // Find all edge intersections with this scanline
    for (size_t i = 0; i < points.size(); ++i) {
      Vec2 p1 = points[i];
      Vec2 p2 = points[(i + 1) % points.size()];

      // Ensure p1.y <= p2.y
      if (p1.y > p2.y) {
        std::swap(p1, p2);
      }

      // Check if edge crosses this scanline
      float fy = static_cast<float>(y);
      if (fy >= p1.y && fy < p2.y) {
        // Calculate x intersection
        float t = (fy - p1.y) / (p2.y - p1.y);
        float x = p1.x + t * (p2.x - p1.x);
        intersections.push_back(x);
      }
    }

    // Sort intersections and draw spans
    std::sort(intersections.begin(), intersections.end());

    for (size_t i = 0; i + 1 < intersections.size(); i += 2) {
      float fy = static_cast<float>(y);
      SDL_RenderLine(renderer_, intersections[i], fy, intersections[i + 1], fy);
    }
  }
}

void ShapeRenderer::fillRect(float x, float y, float w, float h, Color color, float cornerRadius) {
  setColor(color);

  constexpr float kMinCornerRadius = 1.0F;
  if (cornerRadius < kMinCornerRadius) {
    SDL_FRect rect = {x, y, w, h};
    SDL_RenderFillRect(renderer_, &rect);
    return;
  }

  // Clamp corner radius
  constexpr float kHalf = 0.5F;
  float r = std::min(cornerRadius, std::min(w, h) * kHalf);

  // Fill main body (3 rectangles)
  SDL_FRect center = {x + r, y, w - 2 * r, h};
  SDL_FRect left = {x, y + r, r, h - 2 * r};
  SDL_FRect right = {x + w - r, y + r, r, h - 2 * r};

  SDL_RenderFillRect(renderer_, &center);
  SDL_RenderFillRect(renderer_, &left);
  SDL_RenderFillRect(renderer_, &right);

  // Fill corners with quarter circles
  auto quarterCircle = [this](float cx, float cy, float radius, int quadrant) {
    int ir = static_cast<int>(std::lround(radius));
    int icx = static_cast<int>(std::lround(cx));
    int icy = static_cast<int>(std::lround(cy));

    for (int oy = 0; oy <= ir; ++oy) {
      float yNorm = static_cast<float>(oy) / radius;
      float xExtent = radius * std::sqrt(std::max(0.0F, 1.0F - yNorm * yNorm));
      int ox = static_cast<int>(std::lround(xExtent));

      int x1 = 0;
      int x2 = 0;
      int yy = 0;
      switch (quadrant) {
        case 0:  // top-left
          x1 = icx - ox;
          x2 = icx;
          yy = icy - oy;
          break;
        case 1:  // top-right
          x1 = icx;
          x2 = icx + ox;
          yy = icy - oy;
          break;
        case 2:  // bottom-left
          x1 = icx - ox;
          x2 = icx;
          yy = icy + oy;
          break;
        case 3:  // bottom-right
          x1 = icx;
          x2 = icx + ox;
          yy = icy + oy;
          break;
        default:
          return;
      }
      SDL_RenderLine(renderer_, static_cast<float>(x1), static_cast<float>(yy),
                     static_cast<float>(x2), static_cast<float>(yy));
    }
  };

  quarterCircle(x + r, y + r, r, 0);          // top-left
  quarterCircle(x + w - r, y + r, r, 1);      // top-right
  quarterCircle(x + r, y + h - r, r, 2);      // bottom-left
  quarterCircle(x + w - r, y + h - r, r, 3);  // bottom-right
}

void ShapeRenderer::fillGradientV(float x, float y, float w, float h, Color top, Color bottom) {
  int iy = static_cast<int>(y);
  int ih = static_cast<int>(h);

  auto lerpChannel = [](uint8_t a, uint8_t b, float t) -> uint8_t {
    return static_cast<uint8_t>(static_cast<float>(a) +
                                (static_cast<float>(b) - static_cast<float>(a)) * t);
  };

  for (int row = 0; row < ih; ++row) {
    float t = static_cast<float>(row) / static_cast<float>(ih - 1);
    Color c = {lerpChannel(top.r, bottom.r, t), lerpChannel(top.g, bottom.g, t),
               lerpChannel(top.b, bottom.b, t), lerpChannel(top.a, bottom.a, t)};
    setColor(c);
    SDL_RenderLine(renderer_, x, static_cast<float>(iy + row), x + w, static_cast<float>(iy + row));
  }
}

void ShapeRenderer::fillGradientH(float x, float y, float w, float h, Color left, Color right) {
  int ix = static_cast<int>(x);
  int iw = static_cast<int>(w);

  auto lerpChannel = [](uint8_t a, uint8_t b, float t) -> uint8_t {
    return static_cast<uint8_t>(static_cast<float>(a) +
                                (static_cast<float>(b) - static_cast<float>(a)) * t);
  };

  for (int col = 0; col < iw; ++col) {
    float t = static_cast<float>(col) / static_cast<float>(iw - 1);
    Color c = {lerpChannel(left.r, right.r, t), lerpChannel(left.g, right.g, t),
               lerpChannel(left.b, right.b, t), lerpChannel(left.a, right.a, t)};
    setColor(c);
    SDL_RenderLine(renderer_, static_cast<float>(ix + col), y, static_cast<float>(ix + col), y + h);
  }
}

}  // namespace Visual
