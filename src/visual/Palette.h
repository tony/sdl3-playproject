#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>

#include "visual/Rules.h"

namespace Visual {

// Color channel bounds for arithmetic
inline constexpr float kMaxChannel = 255.0F;
inline constexpr float kMinChannel = 0.0F;
inline constexpr uint8_t kMaxChannelU8 = 255;
inline constexpr int kHexDigits = 6;
inline constexpr int kNibbleBits = 4;
inline constexpr int kHexShiftR = 16;
inline constexpr int kHexShiftG = 8;
inline constexpr unsigned int kHexMask = 0xFFU;
inline constexpr int kHexAlphaOffset = 10;

struct Color {
  uint8_t r = 0, g = 0, b = 0, a = kMaxChannelU8;

  constexpr Color() = default;
  constexpr Color(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = kMaxChannelU8)
      : r(r_), g(g_), b(b_), a(a_) {}

  // Parse hex string like "#288CFF" or "288CFF"
  static Color fromHex(std::string_view hex) {
    if (hex.empty()) {
      return {};
    }
    if (hex.front() == '#') {
      hex.remove_prefix(1);
    }
    unsigned int val = 0;
    const size_t len = std::min(static_cast<size_t>(kHexDigits), hex.size());
    for (size_t i = 0; i < len; ++i) {
      val <<= kNibbleBits;
      const char c = hex[i];
      if (c >= '0' && c <= '9') {
        val |= static_cast<unsigned int>(c - '0');
      } else if (c >= 'a' && c <= 'f') {
        val |= static_cast<unsigned int>(c - 'a' + kHexAlphaOffset);
      } else if (c >= 'A' && c <= 'F') {
        val |= static_cast<unsigned int>(c - 'A' + kHexAlphaOffset);
      }
    }
    return {static_cast<uint8_t>((val >> kHexShiftR) & kHexMask),
            static_cast<uint8_t>((val >> kHexShiftG) & kHexMask),
            static_cast<uint8_t>(val & kHexMask)};
  }

  static Color fromHex(const char* hex) {
    if (hex == nullptr) {
      return {};
    }
    return fromHex(std::string_view(hex));
  }
};

// Color temperature shift amounts
inline constexpr float kWarmRedShift = 30.0F;
inline constexpr float kWarmGreenShift = 15.0F;
inline constexpr float kWarmBlueShift = 20.0F;
inline constexpr float kCoolRedShift = 20.0F;
inline constexpr float kCoolGreenShift = 10.0F;
inline constexpr float kCoolBlueShift = 30.0F;

// Lighten a color by factor (0-1)
inline Color lighten(Color c, float amount) {
  auto lift = [amount](uint8_t v) {
    float fv = static_cast<float>(v);
    return static_cast<uint8_t>(std::min(kMaxChannel, fv + (kMaxChannel - fv) * amount));
  };
  return {lift(c.r), lift(c.g), lift(c.b), c.a};
}

// Darken a color by factor (0-1)
inline Color darken(Color c, float amount) {
  auto drop = [amount](uint8_t v) {
    return static_cast<uint8_t>(static_cast<float>(v) * (1.0F - amount));
  };
  return {drop(c.r), drop(c.g), drop(c.b), c.a};
}

// Shift toward warm (yellow/orange)
inline Color shiftWarm(Color c, float amount) {
  return {
      static_cast<uint8_t>(std::min(kMaxChannel, static_cast<float>(c.r) + kWarmRedShift * amount)),
      static_cast<uint8_t>(
          std::min(kMaxChannel, static_cast<float>(c.g) + kWarmGreenShift * amount)),
      static_cast<uint8_t>(
          std::max(kMinChannel, static_cast<float>(c.b) - kWarmBlueShift * amount)),
      c.a};
}

// Shift toward cool (blue/purple)
inline Color shiftCool(Color c, float amount) {
  return {
      static_cast<uint8_t>(std::max(kMinChannel, static_cast<float>(c.r) - kCoolRedShift * amount)),
      static_cast<uint8_t>(
          std::max(kMinChannel, static_cast<float>(c.g) - kCoolGreenShift * amount)),
      static_cast<uint8_t>(
          std::min(kMaxChannel, static_cast<float>(c.b) + kCoolBlueShift * amount)),
      c.a};
}

// Semantic color palette for a character or object
struct Palette {
  Color base;     // the identity color
  Color light;    // top-facing, lit surfaces
  Color dark;     // undersides, creases, far limbs
  Color accent;   // buckle, highlights, eyes
  Color outline;  // silhouette edge (darker than dark)
};

// Derive full palette from just base + accent colors
inline Palette buildPalette(Color base, Color accent, const Rules& rules) {
  constexpr float kLightenAmount = 0.3F;
  constexpr float kWarmShiftAmount = 0.15F;
  constexpr float kDarkenAmount = 0.35F;
  constexpr float kCoolShiftAmount = 0.2F;
  constexpr float kOutlineDarkenAmount = 0.6F;

  Palette p;
  p.base = base;
  p.accent = accent;

  // Light: brighten + warm shift
  p.light = lighten(base, kLightenAmount);
  if (rules.highlightWarm) {
    p.light = shiftWarm(p.light, kWarmShiftAmount);
  }

  // Dark: darken + cool shift
  p.dark = darken(base, kDarkenAmount);
  if (rules.shadowCool) {
    p.dark = shiftCool(p.dark, kCoolShiftAmount);
  }

  // Outline: much darker
  p.outline = darken(base, kOutlineDarkenAmount);

  return p;
}

// Color roles for shape definitions
enum class ColorRole : std::uint8_t { Base, Light, Dark, Accent, Outline, Fixed };

inline Color resolveColorRole(ColorRole role, const Palette& pal, Color fixed = {}) {
  switch (role) {
    case ColorRole::Base:
      return pal.base;
    case ColorRole::Light:
      return pal.light;
    case ColorRole::Dark:
      return pal.dark;
    case ColorRole::Accent:
      return pal.accent;
    case ColorRole::Outline:
      return pal.outline;
    case ColorRole::Fixed:
      return fixed;
  }
  return pal.base;
}

}  // namespace Visual
