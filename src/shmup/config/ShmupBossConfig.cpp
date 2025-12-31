#include "shmup/config/ShmupBossConfig.h"

#include <cstdio>
#include <filesystem>
#include <unordered_set>
#include <utility>

#ifdef SANDBOX_HAS_TOMLPP
#include <toml++/toml.hpp>
#endif

namespace shmup {

namespace {

ShmupBossConfig::MovementType parseMovementType(const std::string& str) {
  if (str == "none")
    return ShmupBossConfig::MovementType::None;
  if (str == "linear")
    return ShmupBossConfig::MovementType::Linear;
  if (str == "sine")
    return ShmupBossConfig::MovementType::Sine;
  if (str == "chase")
    return ShmupBossConfig::MovementType::Chase;
  if (str == "orbit")
    return ShmupBossConfig::MovementType::Orbit;
  if (str == "figure8")
    return ShmupBossConfig::MovementType::Figure8;
  return ShmupBossConfig::MovementType::Sine;
}

ShmupBossConfig::FireType parseFireType(const std::string& str) {
  if (str == "none")
    return ShmupBossConfig::FireType::None;
  if (str == "aimed")
    return ShmupBossConfig::FireType::Aimed;
  if (str == "spread")
    return ShmupBossConfig::FireType::Spread;
  if (str == "circular")
    return ShmupBossConfig::FireType::Circular;
  return ShmupBossConfig::FireType::Spread;
}

#ifdef SANDBOX_HAS_TOMLPP
void parseMovementConfig(const toml::table& tbl, ShmupBossConfig::MovementConfig& move) {
  if (auto val = tbl["type"].value<std::string>()) {
    move.type = parseMovementType(*val);
  }
  if (auto val = tbl["velocity_x"].value<double>()) {
    move.velocityX = static_cast<float>(*val);
  }
  if (auto val = tbl["velocity_y"].value<double>()) {
    move.velocityY = static_cast<float>(*val);
  }
  if (auto val = tbl["amplitude"].value<double>()) {
    move.amplitude = static_cast<float>(*val);
  }
  if (auto val = tbl["frequency"].value<double>()) {
    move.frequency = static_cast<float>(*val);
  }
  if (auto val = tbl["oscillate_y"].value<bool>()) {
    move.oscillateY = *val;
  }
  if (auto val = tbl["chase_speed"].value<double>()) {
    move.chaseSpeed = static_cast<float>(*val);
  }
  if (auto val = tbl["turn_rate"].value<double>()) {
    move.turnRate = static_cast<float>(*val);
  }
  if (auto val = tbl["bounds_min_y"].value<double>()) {
    move.boundsMinY = static_cast<float>(*val);
  }
  if (auto val = tbl["bounds_max_y"].value<double>()) {
    move.boundsMaxY = static_cast<float>(*val);
  }
  if (auto val = tbl["speed_multiplier"].value<double>()) {
    move.speedMultiplier = static_cast<float>(*val);
  }
}

void parseFireConfig(const toml::table& tbl, ShmupBossConfig::FireConfig& fire) {
  if (auto val = tbl["type"].value<std::string>()) {
    fire.type = parseFireType(*val);
  }
  if (auto val = tbl["fire_interval"].value<double>()) {
    fire.fireInterval = static_cast<float>(*val);
  }
  if (auto val = tbl["shot_count"].value<int>()) {
    fire.shotCount = *val;
  }
  if (auto val = tbl["spread_angle"].value<double>()) {
    fire.spreadAngle = static_cast<float>(*val);
  }
  if (auto val = tbl["rotation_speed"].value<double>()) {
    fire.rotationSpeed = static_cast<float>(*val);
  }
  if (auto val = tbl["burst_count"].value<int>()) {
    fire.burstCount = *val;
  }
  if (auto val = tbl["burst_interval"].value<double>()) {
    fire.burstInterval = static_cast<float>(*val);
  }

  // Parse [fire.projectile] section
  if (auto p = tbl["projectile"].as_table()) {
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
  }
}

void parseMinionConfig(const toml::table& tbl, ShmupBossConfig::MinionConfig& minions) {
  if (auto val = tbl["enabled"].value<bool>()) {
    minions.enabled = *val;
  }
  if (auto val = tbl["minion_type"].value<std::string>()) {
    minions.minionType = *val;
  }
  if (auto val = tbl["spawn_interval"].value<double>()) {
    minions.spawnInterval = static_cast<float>(*val);
  }
  if (auto val = tbl["max_active"].value<int>()) {
    minions.maxActive = *val;
  }
  if (auto val = tbl["spawn_offset_x"].value<double>()) {
    minions.spawnOffsetX = static_cast<float>(*val);
  }
  if (auto val = tbl["spawn_offset_y"].value<double>()) {
    minions.spawnOffsetY = static_cast<float>(*val);
  }
}
#endif

}  // namespace

bool ShmupBossConfig::loadFromToml([[maybe_unused]] const char* path) {
#ifndef SANDBOX_HAS_TOMLPP
  std::printf("ShmupBossConfig: toml++ not enabled, cannot load %s\n", path);
  return false;
#else
  std::unordered_set<std::string> seen;

  auto appendFromToml = [&](auto&& self, const std::filesystem::path& filePath) -> bool {
    const std::filesystem::path normalized = filePath.lexically_normal();
    const std::string pathStr = normalized.string();

    if (!seen.insert(pathStr).second) {
      std::printf("ShmupBossConfig: include cycle detected: %s\n", pathStr.c_str());
      return true;
    }

    toml::table tbl;
    try {
      tbl = toml::parse_file(pathStr);
    } catch (const toml::parse_error& err) {
      std::printf("ShmupBossConfig: parse error in %s: %s\n", pathStr.c_str(), err.what());
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
      if (auto val = (*s)["total_health"].value<int>()) {
        stats.totalHealth = *val;
      }
      if (auto val = (*s)["score_value"].value<int>()) {
        stats.scoreValue = *val;
      }
      if (auto val = (*s)["contact_damage"].value<double>()) {
        stats.contactDamage = static_cast<float>(*val);
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

    // Parse [entrance] section
    if (auto e = tbl["entrance"].as_table()) {
      if (auto val = (*e)["start_x"].value<double>()) {
        entrance.startX = static_cast<float>(*val);
      }
      if (auto val = (*e)["start_y"].value<double>()) {
        entrance.startY = static_cast<float>(*val);
      }
      if (auto val = (*e)["target_x"].value<double>()) {
        entrance.targetX = static_cast<float>(*val);
      }
      if (auto val = (*e)["target_y"].value<double>()) {
        entrance.targetY = static_cast<float>(*val);
      }
      if (auto val = (*e)["duration_seconds"].value<double>()) {
        entrance.durationSeconds = static_cast<float>(*val);
      }
      if (auto val = (*e)["invulnerable_during"].value<bool>()) {
        entrance.invulnerableDuring = *val;
      }
    }

    // Parse [death] section
    if (auto d = tbl["death"].as_table()) {
      if (auto val = (*d)["explosion_count"].value<int>()) {
        death.explosionCount = *val;
      }
      if (auto val = (*d)["explosion_interval"].value<double>()) {
        death.explosionInterval = static_cast<float>(*val);
      }
      if (auto val = (*d)["final_explosion_delay"].value<double>()) {
        death.finalExplosionDelay = static_cast<float>(*val);
      }
    }

    // Parse [[phases]] array
    if (auto phasesArr = tbl["phases"].as_array()) {
      for (const auto& phaseNode : *phasesArr) {
        if (auto p = phaseNode.as_table()) {
          PhaseConfig phase;

          if (auto val = (*p)["id"].value<std::string>()) {
            phase.id = *val;
          }
          if (auto val = (*p)["health_threshold"].value<double>()) {
            phase.healthThreshold = static_cast<float>(*val);
          }

          // Parse active weak points array
          if (auto wpArr = (*p)["active_weak_points"].as_array()) {
            for (const auto& wpNode : *wpArr) {
              if (auto wp = wpNode.value<std::string>()) {
                phase.activeWeakPoints.push_back(*wp);
              }
            }
          }

          // Parse phase movement
          if (auto m = (*p)["movement"].as_table()) {
            parseMovementConfig(*m, phase.movement);
          }

          // Parse phase fire
          if (auto f = (*p)["fire"].as_table()) {
            parseFireConfig(*f, phase.fire);
          }

          // Parse phase minions
          if (auto minions = (*p)["minions"].as_table()) {
            parseMinionConfig(*minions, phase.minions);
          }

          phases.push_back(std::move(phase));
        }
      }
    }

    // Parse [[weak_points]] array
    if (auto wpArr = tbl["weak_points"].as_array()) {
      for (const auto& wpNode : *wpArr) {
        if (auto w = wpNode.as_table()) {
          WeakPointConfig wp;

          if (auto val = (*w)["id"].value<std::string>()) {
            wp.id = *val;
          }
          if (auto val = (*w)["offset_x"].value<double>()) {
            wp.offsetX = static_cast<float>(*val);
          }
          if (auto val = (*w)["offset_y"].value<double>()) {
            wp.offsetY = static_cast<float>(*val);
          }
          if (auto val = (*w)["hitbox_w"].value<double>()) {
            wp.hitboxW = static_cast<float>(*val);
          }
          if (auto val = (*w)["hitbox_h"].value<double>()) {
            wp.hitboxH = static_cast<float>(*val);
          }
          if (auto val = (*w)["damage_multiplier"].value<double>()) {
            wp.damageMultiplier = static_cast<float>(*val);
          }
          if (auto val = (*w)["health"].value<int>()) {
            wp.health = *val;
          }
          if (auto val = (*w)["targetable"].value<bool>()) {
            wp.targetable = *val;
          }

          weakPoints.push_back(std::move(wp));
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

const ShmupBossConfig* ShmupBossRegistry::get(const std::string& id) {
  auto it = cache_.find(id);
  return (it != cache_.end()) ? &it->second : nullptr;
}

bool ShmupBossRegistry::load(const std::string& id, const char* path) {
  ShmupBossConfig cfg;
  if (!cfg.loadFromToml(path)) {
    return false;
  }
  cache_[id] = std::move(cfg);
  return true;
}

void ShmupBossRegistry::clear() {
  cache_.clear();
}

}  // namespace shmup
