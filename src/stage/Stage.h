#pragma once

#include <SDL3/SDL_render.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "ecs/Entity.h"

class World;
namespace Visual {
class ShapeRenderer;
}
struct AABB;
struct Grounded;
struct Transform;
struct Velocity;

struct Rect {
  float x = 0.0F;
  float y = 0.0F;
  float w = 0.0F;
  float h = 0.0F;
};

struct SolidRect {
  Rect rect{};
  bool oneWay = false;
};

enum class SlopeDir { UpRight, UpLeft };

struct SlopeRect {
  Rect rect{};
  SlopeDir dir = SlopeDir::UpRight;
};

struct SpawnPoint {
  float x = 0.0F;
  float y = 0.0F;
  int facingX = 1;
};

struct EnemySpawn {
  float x = 0.0F;
  float y = 0.0F;
  int facingX = 1;
  bool hasPatrol = false;
  float patrolMinX = 0.0F;
  float patrolMaxX = 0.0F;
  std::string configPath;
};

struct CollectibleSpawn {
  float x = 0.0F;
  float y = 0.0F;
  std::string configPath;
};

struct Rgba8 {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  uint8_t a = 255;
};

struct StageRenderStyle {
  Rgba8 bgTop{};
  Rgba8 bgBottom{};
  Rgba8 platformBase{};
  Rgba8 platformLight{};
  Rgba8 platformDark{};
  Rgba8 platformHighlight{};
};

enum class ZoneType { Custom, Water, Ice };

struct StageZone {
  Rect rect{};
  ZoneType type = ZoneType::Custom;

  float gravityMultiplier = 1.0F;
  float maxFallSpeedMultiplier = 1.0F;
  float accelMultiplier = 1.0F;
  float maxSpeedMultiplier = 1.0F;
  float frictionMultiplier = 1.0F;
  float groundFrictionMultiplier = 1.0F;
  float airDragMultiplier = 1.0F;
  float turnResistanceMultiplier = 1.0F;
  float jumpImpulseMultiplier = 1.0F;
};

struct StageEnvironmentMultipliers {
  bool inWater = false;
  bool inIce = false;
  float gravity = 1.0F;
  float maxFallSpeed = 1.0F;
  float accel = 1.0F;
  float maxSpeed = 1.0F;
  float friction = 1.0F;
  float groundFriction = 1.0F;
  float airDrag = 1.0F;
  float turnResistance = 1.0F;
  float jumpImpulse = 1.0F;
};

struct StageHazard {
  Rect rect{};
  int iframesMs = 800;
  bool ignoreIframes = false;
  int lockoutMs = 0;
  float knockbackVx = 700.0F;  // magnitude, px/s
  float knockbackVy = 0.0F;    // px/s
};

struct CameraLock {
  Rect trigger{};
  Rect bounds{};
};

class Stage {
 public:
  void loadTestStage();
  bool loadFromToml(const char* stageTomlPath);
  void renderBackground(Visual::ShapeRenderer& shapes, int viewW, int viewH) const;
  void render(SDL_Renderer& r,
              Visual::ShapeRenderer& shapes,
              const World& w,
              float camX,
              float camY) const;
  void renderDebugCollision(SDL_Renderer& r, const World& w, float camX, float camY) const;

  int version() const { return version_; }
  const std::string& id() const { return id_; }
  const std::string& displayName() const { return displayName_; }

  float cameraDeadzoneW() const { return cameraDeadzoneW_; }
  float cameraDeadzoneH() const { return cameraDeadzoneH_; }
  float cameraLookaheadX() const { return cameraLookaheadX_; }
  float cameraLookaheadY() const { return cameraLookaheadY_; }
  int cameraLookHoldMs() const { return cameraLookHoldMs_; }
  float cameraLookUpY() const { return cameraLookUpY_; }
  float cameraLookDownY() const { return cameraLookDownY_; }
  bool cameraNoBackscroll() const { return cameraNoBackscroll_; }

  float collisionGroundSnap() const { return groundSnap_; }
  float collisionStepUp() const { return stepUp_; }
  float collisionSkin() const { return collisionSkin_; }

  const StageRenderStyle& renderStyle() const { return render_; }

  bool getSpawn(const char* name, float& outX, float& outY) const;
  bool getSpawn(const char* name, float& outX, float& outY, int& outFacingX) const;
  std::vector<std::string> spawnNames() const;
  const std::vector<EnemySpawn>& enemySpawns() const { return enemySpawns_; }
  const std::vector<CollectibleSpawn>& collectibleSpawns() const { return collectibleSpawns_; }
  bool getWorldBounds(Rect& out) const;
  bool getCameraBounds(Rect& out) const;
  std::size_t solidCount() const { return solids_.size(); }
  std::size_t slopeCount() const { return slopes_.size(); }
  std::size_t zoneCount() const { return zones_.size(); }
  std::size_t hazardCount() const { return hazards_.size(); }
  std::size_t enemySpawnCount() const { return enemySpawns_.size(); }
  std::size_t collectibleSpawnCount() const { return collectibleSpawns_.size(); }
  std::size_t cameraLockCount() const { return cameraLocks_.size(); }

  StageEnvironmentMultipliers environmentAt(const Rect& aabb) const;
  bool hazardAt(const Rect& aabb, StageHazard& out) const;
  bool overlapsSolid(const Rect& aabb, bool ignoreOneWay = false) const;
  bool cameraLockBoundsAt(const Rect& aabb, Rect& outBounds) const;
  bool touchingWall(const Rect& aabb, int dirX, float probe = 1.0F) const;
  bool groundIsOneWay(const Rect& aabb) const;

  // very simple “solid rectangles” collision
  // Frame-based collision: velocity is in px/frame, integration is pos += vel
  bool resolveAABBCollision(Transform& t,
                            Velocity& v,
                            const AABB& box,
                            Grounded& g,
                            bool ignoreOneWay = false,
                            bool snapCollisionToPixel = false);

  // spawn a demo character from TOML config
  static EntityId spawnDemo(World& w,
                            const char* characterTomlPath,
                            float spawnX = 120.0F,
                            float spawnY = 120.0F,
                            int facingX = 1);
  void spawnEnemies(World& w) const;
  void spawnCollectibles(World& w) const;

 private:
  std::vector<SolidRect> solids_;
  std::vector<SlopeRect> slopes_;
  std::vector<StageZone> zones_;
  std::vector<StageHazard> hazards_;
  std::vector<CameraLock> cameraLocks_;
  std::unordered_map<std::string, SpawnPoint> spawns_;
  std::vector<EnemySpawn> enemySpawns_;
  std::vector<CollectibleSpawn> collectibleSpawns_;

  int version_ = 0;
  std::string id_;
  std::string displayName_;

  float groundSnap_ = 2.0F;
  float stepUp_ = 6.0F;
  float collisionSkin_ = 0.01F;

  bool hasWorldBounds_ = false;
  Rect worldBounds_{};

  bool hasCameraBounds_ = false;
  Rect cameraBounds_{};

  float cameraDeadzoneW_ = 0.0F;
  float cameraDeadzoneH_ = 0.0F;
  float cameraLookaheadX_ = 0.0F;
  float cameraLookaheadY_ = 0.0F;
  int cameraLookHoldMs_ = 0;
  float cameraLookUpY_ = 0.0F;
  float cameraLookDownY_ = 0.0F;
  bool cameraNoBackscroll_ = false;

  StageRenderStyle render_{};
};
