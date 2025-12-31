#pragma once

#include <cstdint>

struct TimeStep {
  float dt = 0.0F;  // seconds
  uint64_t frame = 0;
};
