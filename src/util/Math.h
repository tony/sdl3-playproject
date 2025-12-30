#pragma once

#include <cmath>

#include "character/CharacterConfig.h"

namespace util {

inline float quantizeToGrid(float v, int subpixel, CharacterConfig::Math::QuantizeMode mode) {
  if (subpixel <= 1)
    return v;
  const float scale = static_cast<float>(subpixel);
  const float x = v * scale;
  switch (mode) {
    case CharacterConfig::Math::QuantizeMode::Trunc:
      return std::trunc(x) / scale;
    case CharacterConfig::Math::QuantizeMode::Round:
      break;
  }
  return std::round(x) / scale;
}

}  // namespace util
