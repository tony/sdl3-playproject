#include "ecs/World.h"

#include "ecs/Systems.h"

EntityId World::create() {
  return registry.create();
}

void World::destroy(EntityId id) {
  registry.destroy(id);
}

void World::update(Stage& stage, TimeStep ts) {
  Systems::characterController(*this, stage, ts);
  Systems::enemies(*this, stage, ts);
  Systems::integrate(*this, stage, ts);
  Systems::attackHitboxes(*this, stage, ts);
  Systems::projectiles(*this, stage, ts);
  Systems::hazards(*this, stage, ts);
  Systems::collectibles(*this, stage, ts);
  Systems::combat(*this, stage, ts);
  Systems::deriveAnimState(*this);
}
