#include "shmup/config/ShipConfig.h"

#include <cstdio>
#include <filesystem>
#include <unordered_set>
#include <utility>

#ifdef SANDBOX_HAS_TOMLPP
#include <toml++/toml.hpp>
#endif

namespace shmup {

bool ShipConfig::loadFromToml([[maybe_unused]] const char* path) {
#ifndef SANDBOX_HAS_TOMLPP
  std::printf("ShipConfig: toml++ not enabled, cannot load %s\n", path);
  return false;
#else
  std::unordered_set<std::string> seen;  // Cycle detection for includes

  auto appendFromToml = [&](auto&& self, const std::filesystem::path& filePath) -> bool {
    const std::filesystem::path normalized = filePath.lexically_normal();
    const std::string pathStr = normalized.string();

    // Cycle detection
    if (!seen.insert(pathStr).second) {
      std::printf("ShipConfig: include cycle detected: %s\n", pathStr.c_str());
      return true;  // Continue, don't fail
    }

    toml::table tbl;
    try {
      tbl = toml::parse_file(pathStr);
    } catch (const toml::parse_error& err) {
      std::printf("ShipConfig: parse error in %s: %s\n", pathStr.c_str(), err.what());
      return false;
    }

    // Process includes (single string or array)
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

    // Parse [physics] section
    if (auto p = tbl["physics"].as_table()) {
      if (auto val = (*p)["speed"].value<double>()) {
        physics.speed = static_cast<float>(*val);
      }
      if (auto val = (*p)["focused_speed"].value<double>()) {
        physics.focusedSpeed = static_cast<float>(*val);
      }
      if (auto val = (*p)["accel"].value<double>()) {
        physics.accel = static_cast<float>(*val);
      }
      if (auto val = (*p)["instant_movement"].value<bool>()) {
        physics.instantMovement = *val;
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
      if (auto val = (*c)["graze_w"].value<double>()) {
        collision.grazeW = static_cast<float>(*val);
      }
      if (auto val = (*c)["graze_h"].value<double>()) {
        collision.grazeH = static_cast<float>(*val);
      }
    }

    // Parse [[weapon_slots]] array
    if (auto slots = tbl["weapon_slots"].as_array()) {
      for (const auto& slot : *slots) {
        if (auto t = slot.as_table()) {
          WeaponSlot ws;
          if (auto val = (*t)["offset_x"].value<double>()) {
            ws.offsetX = static_cast<float>(*val);
          }
          if (auto val = (*t)["offset_y"].value<double>()) {
            ws.offsetY = static_cast<float>(*val);
          }
          if (auto val = (*t)["default_weapon"].value<std::string>()) {
            ws.defaultWeapon = *val;
          }
          if (auto val = (*t)["default_level"].value<int>()) {
            ws.defaultLevel = *val;
          }
          weaponSlots.push_back(ws);
        }
      }
    }

    // Parse [[satellite_slots]] array
    if (auto slots = tbl["satellite_slots"].as_array()) {
      for (const auto& slot : *slots) {
        if (auto t = slot.as_table()) {
          SatelliteSlot ss;
          if (auto val = (*t)["base_offset_x"].value<double>()) {
            ss.baseOffsetX = static_cast<float>(*val);
          }
          if (auto val = (*t)["base_offset_y"].value<double>()) {
            ss.baseOffsetY = static_cast<float>(*val);
          }
          if (auto val = (*t)["orbit_radius"].value<double>()) {
            ss.orbitRadius = static_cast<float>(*val);
          }
          if (auto val = (*t)["orbit_speed"].value<double>()) {
            ss.orbitSpeed = static_cast<float>(*val);
          }
          if (auto val = (*t)["focused_offset_x"].value<double>()) {
            ss.focusedOffsetX = static_cast<float>(*val);
          }
          if (auto val = (*t)["focused_offset_y"].value<double>()) {
            ss.focusedOffsetY = static_cast<float>(*val);
          }
          if (auto val = (*t)["default_satellite"].value<std::string>()) {
            ss.defaultSatellite = *val;
          }
          satelliteSlots.push_back(ss);
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

const ShipConfig* ShipRegistry::get(const std::string& id) {
  auto it = cache_.find(id);
  return (it != cache_.end()) ? &it->second : nullptr;
}

bool ShipRegistry::load(const std::string& id, const char* path) {
  ShipConfig cfg;
  if (!cfg.loadFromToml(path)) {
    return false;
  }
  cache_[id] = std::move(cfg);
  return true;
}

void ShipRegistry::clear() {
  cache_.clear();
}

}  // namespace shmup
