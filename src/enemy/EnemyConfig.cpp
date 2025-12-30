#include "enemy/EnemyConfig.h"

#include <algorithm>
#include <format>
#include <string_view>
#include <utility>

#include <toml++/toml.h>

#include "util/TomlUtil.h"

namespace {

EnemyConfig::Type parseType(std::string_view value, const char* path) {
  if (value == "walker") {
    return EnemyConfig::Type::Walker;
  }
  if (value == "spiky") {
    return EnemyConfig::Type::Spiky;
  }
  if (value == "shooter") {
    return EnemyConfig::Type::Shooter;
  }
  if (value == "hopper") {
    return EnemyConfig::Type::Hopper;
  }
  TomlUtil::warnf(path, "enemy.type must be one of walker|spiky|shooter|hopper (got '{}')",
                  std::string(value));
  return EnemyConfig::Type::Walker;
}

float clampNonNegative(float v) {
  return std::max(0.0F, v);
}

int clampNonNegative(int v) {
  return std::max(0, v);
}

}  // namespace

bool EnemyConfig::loadFromToml(const char* path) {
  toml::table tbl;
  try {
    tbl = toml::parse_file(path);
  } catch (...) {
    return false;
  }

  TomlUtil::warnUnknownKeys(
      tbl, path, "root",
      {"version", "enemy", "collision", "move", "combat", "hopper", "shooter", "render"});

  EnemyConfig next{};

  if (auto v = tbl["version"].value<int>())
    next.version = *v;

  if (auto e = tbl["enemy"].as_table()) {
    TomlUtil::warnUnknownKeys(*e, path, "enemy", {"id", "display", "type"});
    if (auto v = e->get("id"))
      next.id = v->value_or(next.id);
    if (auto v = e->get("display"))
      next.displayName = v->value_or(next.displayName);
    if (auto v = e->get("type")) {
      if (auto s = v->value<std::string_view>()) {
        next.type = parseType(*s, path);
      }
    }
  }
  if (next.displayName.empty())
    next.displayName = next.id;

  if (auto c = tbl["collision"].as_table()) {
    TomlUtil::warnUnknownKeys(*c, path, "collision", {"w", "h"});
    if (auto v = c->get("w"))
      next.collision.w = v->value_or(next.collision.w);
    if (auto v = c->get("h"))
      next.collision.h = v->value_or(next.collision.h);
  }

  if (auto m = tbl["move"].as_table()) {
    TomlUtil::warnUnknownKeys(
        *m, path, "move", {"speed", "gravity", "max_fall_speed", "turn_on_wall", "turn_on_edge"});
    if (auto v = m->get("speed"))
      next.move.speed = v->value_or(next.move.speed);
    if (auto v = m->get("gravity"))
      next.move.gravity = v->value_or(next.move.gravity);
    if (auto v = m->get("max_fall_speed"))
      next.move.maxFallSpeed = v->value_or(next.move.maxFallSpeed);
    if (auto v = m->get("turn_on_wall"))
      next.move.turnOnWall = v->value_or(next.move.turnOnWall);
    if (auto v = m->get("turn_on_edge"))
      next.move.turnOnEdge = v->value_or(next.move.turnOnEdge);
  }

  if (auto c = tbl["combat"].as_table()) {
    TomlUtil::warnUnknownKeys(*c, path, "combat",
                              {"health", "contact_damage", "stompable", "stomp_bounce_vy",
                               "iframes_ms", "knockback_vx", "knockback_vy"});
    if (auto v = c->get("health"))
      next.combat.health = v->value_or(next.combat.health);
    if (auto v = c->get("contact_damage"))
      next.combat.contactDamage = v->value_or(next.combat.contactDamage);
    if (auto v = c->get("stompable"))
      next.combat.stompable = v->value_or(next.combat.stompable);
    if (auto v = c->get("stomp_bounce_vy"))
      next.combat.stompBounceVy = v->value_or(next.combat.stompBounceVy);
    if (auto v = c->get("iframes_ms"))
      next.combat.iframesMs = v->value_or(next.combat.iframesMs);
    if (auto v = c->get("knockback_vx"))
      next.combat.knockbackVx = v->value_or(next.combat.knockbackVx);
    if (auto v = c->get("knockback_vy"))
      next.combat.knockbackVy = v->value_or(next.combat.knockbackVy);
  }

  if (auto h = tbl["hopper"].as_table()) {
    TomlUtil::warnUnknownKeys(*h, path, "hopper",
                              {"interval_frames", "hop_speed_x", "hop_speed_y"});
    if (auto v = h->get("interval_frames"))
      next.hopper.intervalFrames = v->value_or(next.hopper.intervalFrames);
    if (auto v = h->get("hop_speed_x"))
      next.hopper.hopSpeedX = v->value_or(next.hopper.hopSpeedX);
    if (auto v = h->get("hop_speed_y"))
      next.hopper.hopSpeedY = v->value_or(next.hopper.hopSpeedY);
  }

  if (auto s = tbl["shooter"].as_table()) {
    TomlUtil::warnUnknownKeys(*s, path, "shooter",
                              {"fire_interval_frames", "warmup_frames", "projectile"});
    if (auto v = s->get("fire_interval_frames"))
      next.shooter.fireIntervalFrames = v->value_or(next.shooter.fireIntervalFrames);
    if (auto v = s->get("warmup_frames"))
      next.shooter.warmupFrames = v->value_or(next.shooter.warmupFrames);
    if (auto p = (*s)["projectile"].as_table()) {
      TomlUtil::warnUnknownKeys(*p, path, "shooter.projectile",
                                {"speed", "gravity", "lifetime_frames", "w", "h", "offset_x",
                                 "offset_y", "damage", "knockback_vx", "knockback_vy"});
      if (auto v = p->get("speed"))
        next.shooter.projectile.speed = v->value_or(next.shooter.projectile.speed);
      if (auto v = p->get("gravity"))
        next.shooter.projectile.gravity = v->value_or(next.shooter.projectile.gravity);
      if (auto v = p->get("lifetime_frames"))
        next.shooter.projectile.lifetimeFrames =
            v->value_or(next.shooter.projectile.lifetimeFrames);
      if (auto v = p->get("w"))
        next.shooter.projectile.w = v->value_or(next.shooter.projectile.w);
      if (auto v = p->get("h"))
        next.shooter.projectile.h = v->value_or(next.shooter.projectile.h);
      if (auto v = p->get("offset_x"))
        next.shooter.projectile.offsetX = v->value_or(next.shooter.projectile.offsetX);
      if (auto v = p->get("offset_y"))
        next.shooter.projectile.offsetY = v->value_or(next.shooter.projectile.offsetY);
      if (auto v = p->get("damage"))
        next.shooter.projectile.damage = v->value_or(next.shooter.projectile.damage);
      if (auto v = p->get("knockback_vx"))
        next.shooter.projectile.knockbackVx = v->value_or(next.shooter.projectile.knockbackVx);
      if (auto v = p->get("knockback_vy"))
        next.shooter.projectile.knockbackVy = v->value_or(next.shooter.projectile.knockbackVy);
    }
  }

  // Parse [render] section for sprite-based rendering
  if (auto r = tbl["render"].as_table()) {
    TomlUtil::warnUnknownKeys(
        *r, path, "render",
        {"sheets", "frame_w", "frame_h", "scale", "offset_x", "offset_y", "anims"});
    if (auto v = r->get("frame_w"))
      next.render.frameW = v->value_or(next.render.frameW);
    if (auto v = r->get("frame_h"))
      next.render.frameH = v->value_or(next.render.frameH);
    if (auto v = r->get("scale"))
      next.render.scale = v->value_or(next.render.scale);
    if (auto v = r->get("offset_x"))
      next.render.offsetX = v->value_or(next.render.offsetX);
    if (auto v = r->get("offset_y"))
      next.render.offsetY = v->value_or(next.render.offsetY);

    // Parse [render.sheets] - direction -> path mapping
    if (auto sheetsTable = (*r)["sheets"].as_table()) {
      for (const auto& [key, node] : *sheetsTable) {
        if (auto pathStr = node.value<std::string>()) {
          next.render.sheets[std::string(key)] = *pathStr;
        }
      }
    }

    // Parse [render.anims.*] - animation clips
    if (auto animsTable = (*r)["anims"].as_table()) {
      for (const auto& [animName, animNode] : *animsTable) {
        if (auto animTable = animNode.as_table()) {
          RenderClip clip{};
          if (auto v = animTable->get("row"))
            clip.row = v->value_or(clip.row);
          if (auto v = animTable->get("start"))
            clip.start = v->value_or(clip.start);
          if (auto v = animTable->get("frames"))
            clip.frames = v->value_or(clip.frames);
          if (auto v = animTable->get("fps"))
            clip.fps = v->value_or(clip.fps);
          next.render.anims[std::string(animName)] = clip;
        }
      }
    }
  }

  next.move.speed = clampNonNegative(next.move.speed);
  next.move.gravity = clampNonNegative(next.move.gravity);
  next.move.maxFallSpeed = clampNonNegative(next.move.maxFallSpeed);

  next.combat.health = std::max(1, next.combat.health);
  next.combat.contactDamage = clampNonNegative(next.combat.contactDamage);
  next.combat.iframesMs = clampNonNegative(next.combat.iframesMs);

  next.hopper.intervalFrames = clampNonNegative(next.hopper.intervalFrames);

  next.shooter.fireIntervalFrames = clampNonNegative(next.shooter.fireIntervalFrames);
  next.shooter.warmupFrames = clampNonNegative(next.shooter.warmupFrames);
  next.shooter.projectile.speed = clampNonNegative(next.shooter.projectile.speed);
  next.shooter.projectile.lifetimeFrames = clampNonNegative(next.shooter.projectile.lifetimeFrames);
  next.shooter.projectile.w = clampNonNegative(next.shooter.projectile.w);
  next.shooter.projectile.h = clampNonNegative(next.shooter.projectile.h);
  next.shooter.projectile.damage = clampNonNegative(next.shooter.projectile.damage);

  *this = std::move(next);
  return true;
}
