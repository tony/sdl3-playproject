#include "shmup/config/ShmupEnemyConfig.h"

#include <cstdio>
#include <filesystem>
#include <unordered_set>
#include <utility>

#ifdef SANDBOX_HAS_TOMLPP
#include <toml++/toml.hpp>
#endif

namespace shmup {

namespace {

ShmupEnemyConfig::MovementType parseMovementType(const std::string& str) {
  if (str == "none")
    return ShmupEnemyConfig::MovementType::None;
  if (str == "linear")
    return ShmupEnemyConfig::MovementType::Linear;
  if (str == "sine")
    return ShmupEnemyConfig::MovementType::Sine;
  if (str == "chase")
    return ShmupEnemyConfig::MovementType::Chase;
  if (str == "orbit")
    return ShmupEnemyConfig::MovementType::Orbit;
  if (str == "formation")
    return ShmupEnemyConfig::MovementType::Formation;
  return ShmupEnemyConfig::MovementType::Linear;
}

ShmupEnemyConfig::FireType parseFireType(const std::string& str) {
  if (str == "none")
    return ShmupEnemyConfig::FireType::None;
  if (str == "aimed")
    return ShmupEnemyConfig::FireType::Aimed;
  if (str == "spread")
    return ShmupEnemyConfig::FireType::Spread;
  if (str == "circular")
    return ShmupEnemyConfig::FireType::Circular;
  return ShmupEnemyConfig::FireType::None;
}

}  // namespace

bool ShmupEnemyConfig::loadFromToml([[maybe_unused]] const char* path) {
#ifndef SANDBOX_HAS_TOMLPP
  std::printf("ShmupEnemyConfig: toml++ not enabled, cannot load %s\n", path);
  return false;
#else
  std::unordered_set<std::string> seen;

  auto appendFromToml = [&](auto&& self, const std::filesystem::path& filePath) -> bool {
    const std::filesystem::path normalized = filePath.lexically_normal();
    const std::string pathStr = normalized.string();

    if (!seen.insert(pathStr).second) {
      std::printf("ShmupEnemyConfig: include cycle detected: %s\n", pathStr.c_str());
      return true;
    }

    toml::table tbl;
    try {
      tbl = toml::parse_file(pathStr);
    } catch (const toml::parse_error& err) {
      std::printf("ShmupEnemyConfig: parse error in %s: %s\n", pathStr.c_str(), err.what());
      return false;
    }

    // Process includes
    if (auto include = tbl["include"].value<std::string>()) {
      const std::filesystem::path includePath = normalized.parent_path() / *include;
      if (!self(self, includePath)) {
        return false;
      }
    } else if (auto includes = tbl["include"].as_array()) {
      for (const auto& node : *includes) {
        if (auto includePathStr = node.value<std::string>()) {
          const std::filesystem::path includePath = normalized.parent_path() / *includePathStr;
          if (!self(self, includePath)) {
            return false;
          }
        }
      }
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

    // Parse [stats] section
    if (auto s = tbl["stats"].as_table()) {
      if (auto val = (*s)["health"].value<int>()) {
        stats.health = *val;
      }
      if (auto val = (*s)["score_value"].value<int>()) {
        stats.scoreValue = *val;
      }
      if (auto val = (*s)["contact_damage"].value<double>()) {
        stats.contactDamage = static_cast<float>(*val);
      }
      if (auto val = (*s)["invulnerable"].value<bool>()) {
        stats.invulnerable = *val;
      }
    }

    // Parse [collision] section
    if (auto c = tbl["collision"].as_table()) {
      if (auto val = (*c)["w"].value<double>()) {
        collision.w = static_cast<float>(*val);
      }
      if (auto val = (*c)["h"].value<double>()) {
        collision.h = static_cast<float>(*val);
      }
    }

    // Parse [movement] section
    if (auto m = tbl["movement"].as_table()) {
      if (auto val = (*m)["type"].value<std::string>()) {
        movement.type = parseMovementType(*val);
      }
      if (auto val = (*m)["velocity_x"].value<double>()) {
        movement.velocityX = static_cast<float>(*val);
      }
      if (auto val = (*m)["velocity_y"].value<double>()) {
        movement.velocityY = static_cast<float>(*val);
      }
      if (auto val = (*m)["amplitude"].value<double>()) {
        movement.amplitude = static_cast<float>(*val);
      }
      if (auto val = (*m)["frequency"].value<double>()) {
        movement.frequency = static_cast<float>(*val);
      }
      if (auto val = (*m)["oscillate_y"].value<bool>()) {
        movement.oscillateY = *val;
      }
      if (auto val = (*m)["chase_speed"].value<double>()) {
        movement.chaseSpeed = static_cast<float>(*val);
      }
      if (auto val = (*m)["turn_rate"].value<double>()) {
        movement.turnRate = static_cast<float>(*val);
      }
      if (auto val = (*m)["orbit_radius"].value<double>()) {
        movement.orbitRadius = static_cast<float>(*val);
      }
      if (auto val = (*m)["orbit_speed"].value<double>()) {
        movement.orbitSpeed = static_cast<float>(*val);
      }
    }

    // Parse [fire] section
    if (auto f = tbl["fire"].as_table()) {
      if (auto val = (*f)["type"].value<std::string>()) {
        fire.type = parseFireType(*val);
      }
      if (auto val = (*f)["fire_interval"].value<double>()) {
        fire.fireInterval = static_cast<float>(*val);
      }
      if (auto val = (*f)["warmup_frames"].value<int>()) {
        fire.warmupFrames = *val;
      }
      if (auto val = (*f)["shot_count"].value<int>()) {
        fire.shotCount = *val;
      }
      if (auto val = (*f)["spread_angle"].value<double>()) {
        fire.spreadAngle = static_cast<float>(*val);
      }
      if (auto val = (*f)["rotation_speed"].value<double>()) {
        fire.rotationSpeed = static_cast<float>(*val);
      }

      // Parse [fire.projectile] section
      if (auto p = (*f)["projectile"].as_table()) {
        if (auto val = (*p)["speed"].value<double>()) {
          fire.projectile.speed = static_cast<float>(*val);
        }
        if (auto val = (*p)["damage"].value<double>()) {
          fire.projectile.damage = static_cast<float>(*val);
        }
        if (auto val = (*p)["lifetime_frames"].value<int>()) {
          fire.projectile.lifetimeFrames = *val;
        }
        if (auto val = (*p)["w"].value<double>()) {
          fire.projectile.w = static_cast<float>(*val);
        }
        if (auto val = (*p)["h"].value<double>()) {
          fire.projectile.h = static_cast<float>(*val);
        }
        if (auto val = (*p)["gravity"].value<double>()) {
          fire.projectile.gravity = static_cast<float>(*val);
        }
      }
    }

    // Parse [render] section
    if (auto r = tbl["render"].as_table()) {
      if (auto val = (*r)["frame_w"].value<int>()) {
        render.frameW = *val;
      }
      if (auto val = (*r)["frame_h"].value<int>()) {
        render.frameH = *val;
      }
      if (auto val = (*r)["scale"].value<double>()) {
        render.scale = static_cast<float>(*val);
      }
      if (auto val = (*r)["offset_x"].value<double>()) {
        render.offsetX = static_cast<float>(*val);
      }
      if (auto val = (*r)["offset_y"].value<double>()) {
        render.offsetY = static_cast<float>(*val);
      }

      // Parse [render.sheets]
      if (auto sheets = (*r)["sheets"].as_table()) {
        for (auto&& [key, val] : *sheets) {
          if (auto s = val.value<std::string>()) {
            render.sheets[std::string(key.str())] = *s;
          }
        }
      }

      // Parse [render.anims.*]
      if (auto anims = (*r)["anims"].as_table()) {
        for (auto&& [key, val] : *anims) {
          if (auto a = val.as_table()) {
            Render::AnimClip clip;
            if (auto v = (*a)["row"].value<int>()) {
              clip.row = *v;
            }
            if (auto v = (*a)["start"].value<int>()) {
              clip.start = *v;
            }
            if (auto v = (*a)["frames"].value<int>()) {
              clip.frames = *v;
            }
            if (auto v = (*a)["fps"].value<double>()) {
              clip.fps = static_cast<float>(*v);
            }
            render.anims[std::string(key.str())] = clip;
          }
        }
      }
    }

    return true;
  };

  return appendFromToml(appendFromToml, path);
#endif
}

const ShmupEnemyConfig* ShmupEnemyRegistry::get(const std::string& id) {
  auto it = cache_.find(id);
  return (it != cache_.end()) ? &it->second : nullptr;
}

bool ShmupEnemyRegistry::load(const std::string& id, const char* path) {
  ShmupEnemyConfig cfg;
  if (!cfg.loadFromToml(path)) {
    return false;
  }
  cache_[id] = std::move(cfg);
  return true;
}

void ShmupEnemyRegistry::clear() {
  cache_.clear();
}

}  // namespace shmup
