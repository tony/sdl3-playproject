#include "collectible/CollectibleConfig.h"

#include <algorithm>
#include <string_view>
#include <utility>

#include <toml++/toml.h>

#include "util/TomlUtil.h"

namespace {

CollectibleConfig::Type parseType(std::string_view value, const char* path) {
  if (value == "coin") {
    return CollectibleConfig::Type::Coin;
  }
  if (value == "gem") {
    return CollectibleConfig::Type::Gem;
  }
  if (value == "health") {
    return CollectibleConfig::Type::Health;
  }
  if (value == "powerup") {
    return CollectibleConfig::Type::Powerup;
  }
  TomlUtil::warnf(path, "collectible.type must be one of coin|gem|health|powerup (got '{}')",
                  std::string(value));
  return CollectibleConfig::Type::Coin;
}

float clampNonNegative(float v) {
  return std::max(0.0F, v);
}

int clampNonNegative(int v) {
  return std::max(0, v);
}

}  // namespace

bool CollectibleConfig::loadFromToml(const char* path) {
  toml::table tbl;
  try {
    tbl = toml::parse_file(path);
  } catch (...) {
    return false;
  }

  TomlUtil::warnUnknownKeys(tbl, path, "root",
                            {"version", "collectible", "collision", "value", "render"});

  CollectibleConfig next{};

  if (auto v = tbl["version"].value<int>())
    next.version = *v;

  if (auto c = tbl["collectible"].as_table()) {
    TomlUtil::warnUnknownKeys(*c, path, "collectible", {"id", "display", "type"});
    if (auto v = c->get("id"))
      next.id = v->value_or(next.id);
    if (auto v = c->get("display"))
      next.displayName = v->value_or(next.displayName);
    if (auto v = c->get("type")) {
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

  if (auto val = tbl["value"].as_table()) {
    TomlUtil::warnUnknownKeys(*val, path, "value", {"score", "health"});
    if (auto v = val->get("score"))
      next.value.score = v->value_or(next.value.score);
    if (auto v = val->get("health"))
      next.value.health = v->value_or(next.value.health);
  }

  if (auto r = tbl["render"].as_table()) {
    TomlUtil::warnUnknownKeys(
        *r, path, "render",
        {"sprite", "frame_w", "frame_h", "frames", "fps", "scale", "offset_x", "offset_y"});
    if (auto v = r->get("sprite"))
      next.render.sprite = v->value_or(next.render.sprite);
    if (auto v = r->get("frame_w"))
      next.render.frameW = v->value_or(next.render.frameW);
    if (auto v = r->get("frame_h"))
      next.render.frameH = v->value_or(next.render.frameH);
    if (auto v = r->get("frames"))
      next.render.frames = v->value_or(next.render.frames);
    if (auto v = r->get("fps"))
      next.render.fps = v->value_or(next.render.fps);
    if (auto v = r->get("scale"))
      next.render.scale = v->value_or(next.render.scale);
    if (auto v = r->get("offset_x"))
      next.render.offsetX = v->value_or(next.render.offsetX);
    if (auto v = r->get("offset_y"))
      next.render.offsetY = v->value_or(next.render.offsetY);
  }

  next.collision.w = clampNonNegative(next.collision.w);
  next.collision.h = clampNonNegative(next.collision.h);
  next.value.score = clampNonNegative(next.value.score);
  next.render.frameW = clampNonNegative(next.render.frameW);
  next.render.frameH = clampNonNegative(next.render.frameH);
  next.render.frames = std::max(1, next.render.frames);
  next.render.fps = clampNonNegative(next.render.fps);
  next.render.scale = clampNonNegative(next.render.scale);

  *this = std::move(next);
  return true;
}
