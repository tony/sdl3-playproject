#pragma once

class Stage;
class World;
struct TimeStep;

namespace Systems {
void characterController(World& w, Stage& s, TimeStep ts);
void enemies(World& w, Stage& s, TimeStep ts);
void integrate(World& w, Stage& s, TimeStep ts);
void attackHitboxes(World& w, Stage& s, TimeStep ts);
void projectiles(World& w, Stage& s, TimeStep ts);
void hazards(World& w, Stage& s, TimeStep ts);
void collectibles(World& w, Stage& s, TimeStep ts);
void combat(World& w, Stage& s, TimeStep ts);
void deriveAnimState(World& w);
}  // namespace Systems
