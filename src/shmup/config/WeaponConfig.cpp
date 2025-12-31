#include "shmup/config/WeaponConfig.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <utility>

#ifdef SANDBOX_HAS_TOMLPP
#include <toml++/toml.hpp>
#endif

namespace shmup {

namespace {

WeaponConfig::PatternType parsePatternType(std::string_view s) {
  if (s == "single")
    return WeaponConfig::PatternType::Single;
  if (s == "spread")
    return WeaponConfig::PatternType::Spread;
  if (s == "laser")
    return WeaponConfig::PatternType::Laser;
  if (s == "homing")
    return WeaponConfig::PatternType::Homing;
  if (s == "wave")
    return WeaponConfig::PatternType::Wave;
  return WeaponConfig::PatternType::Single;
}

}  // namespace

bool WeaponConfig::loadFromToml([[maybe_unused]] const char* path) {
#ifndef SANDBOX_HAS_TOMLPP
  std::printf("WeaponConfig: toml++ not enabled, cannot load %s\n", path);
  return false;
#else
  toml::table tbl;
  try {
    tbl = toml::parse_file(path);
  } catch (const toml::parse_error& err) {
    std::printf("WeaponConfig: parse error in %s: %s\n", path, err.what());
    return false;
  }

  // Parse version
  if (auto v = tbl["version"].value<int>()) {
    version = *v;
  }

  // Parse [name] section
  if (auto n = tbl["name"].as_table()) {
    if (auto val = (*n)["id"].value<std::string>()) {
      id = *val;
    }
    if (auto val = (*n)["display"].value<std::string>()) {
      displayName = *val;
    }
  }

  // Parse pattern type
  if (auto p = tbl["pattern"].value<std::string>()) {
    pattern = parsePatternType(*p);
  }

  // Parse [[levels]] array
  if (auto lvls = tbl["levels"].as_array()) {
    for (const auto& lvl : *lvls) {
      if (auto t = lvl.as_table()) {
        LevelStats stats;
        if (auto v = (*t)["level"].value<int>()) {
          stats.level = *v;
        }
        if (auto v = (*t)["projectile_count"].value<int>()) {
          stats.projectileCount = *v;
        }
        if (auto v = (*t)["spread_angle"].value<double>()) {
          stats.spreadAngle = static_cast<float>(*v);
        }
        if (auto v = (*t)["cooldown_frames"].value<int>()) {
          stats.cooldownFrames = *v;
        }
        if (auto v = (*t)["damage"].value<double>()) {
          stats.damage = static_cast<float>(*v);
        }
        if (auto v = (*t)["pierce"].value<int>()) {
          stats.pierce = *v;
        }
        if (auto v = (*t)["projectile_speed"].value<double>()) {
          stats.projectileSpeed = static_cast<float>(*v);
        }
        levels.push_back(stats);
      }
    }
  }

  // Parse [projectile] section
  if (auto p = tbl["projectile"].as_table()) {
    if (auto v = (*p)["w"].value<double>()) {
      projectile.w = static_cast<float>(*v);
    }
    if (auto v = (*p)["h"].value<double>()) {
      projectile.h = static_cast<float>(*v);
    }
    if (auto v = (*p)["lifetime_frames"].value<int>()) {
      projectile.lifetimeFrames = *v;
    }
    if (auto v = (*p)["gravity"].value<double>()) {
      projectile.gravity = static_cast<float>(*v);
    }
    if (auto v = (*p)["sprite"].value<std::string>()) {
      projectile.sprite = *v;
    }
    if (auto v = (*p)["homing_turn_rate"].value<double>()) {
      projectile.homingTurnRate = static_cast<float>(*v);
    }
    if (auto v = (*p)["homing_seek_radius"].value<double>()) {
      projectile.homingSeekRadius = static_cast<float>(*v);
    }
    if (auto v = (*p)["homing_delay_frames"].value<int>()) {
      projectile.homingDelayFrames = *v;
    }
  }

  // Parse [laser] section
  if (auto l = tbl["laser"].as_table()) {
    if (auto v = (*l)["width"].value<double>()) {
      laser.width = static_cast<float>(*v);
    }
    if (auto v = (*l)["max_length"].value<double>()) {
      laser.maxLength = static_cast<float>(*v);
    }
    if (auto v = (*l)["extension_rate"].value<double>()) {
      laser.extensionRate = static_cast<float>(*v);
    }
    if (auto v = (*l)["damage_per_frame"].value<double>()) {
      laser.damagePerFrame = static_cast<float>(*v);
    }
  }

  // Parse [audio] section
  if (auto a = tbl["audio"].as_table()) {
    if (auto v = (*a)["fire_sound"].value<std::string>()) {
      audio.fireSound = *v;
    }
    if (auto v = (*a)["hit_sound"].value<std::string>()) {
      audio.hitSound = *v;
    }
  }

  return true;
#endif
}

const WeaponConfig::LevelStats& WeaponConfig::getLevel(int level) const {
  if (levels.empty()) {
    static const LevelStats defaultStats{};
    return defaultStats;
  }
  int idx = std::clamp(level - 1, 0, static_cast<int>(levels.size()) - 1);
  return levels[static_cast<std::size_t>(idx)];
}

const WeaponConfig* WeaponRegistry::get(const std::string& id) {
  auto it = cache_.find(id);
  return (it != cache_.end()) ? &it->second : nullptr;
}

bool WeaponRegistry::load(const std::string& id, const char* path) {
  WeaponConfig cfg;
  if (!cfg.loadFromToml(path)) {
    return false;
  }
  cache_[id] = std::move(cfg);
  return true;
}

void WeaponRegistry::clear() {
  cache_.clear();
}

}  // namespace shmup
