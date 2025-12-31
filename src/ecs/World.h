#pragma once

#include <cstdint>
#include <entt/entt.hpp>

#include "core/Time.h"       // IWYU pragma: keep
#include "ecs/Components.h"  // IWYU pragma: keep
#include "ecs/Entity.h"

class Stage;

class World {
 public:
  EntityId create();
  void destroy(EntityId);

  void update(Stage& stage, TimeStep ts);

  // EnTT registry replaces the 22+ unordered_maps
  entt::registry registry;

  // debug/test-friendly counters
  int hurtEvents = 0;
  int enemyKills = 0;
  std::int64_t score = 0;

  // external handle: "currently controlled player"
  EntityId player = kInvalidEntity;
};
