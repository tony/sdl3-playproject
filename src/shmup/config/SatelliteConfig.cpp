#include "shmup/config/SatelliteConfig.h"

#include <cstdio>
#include <utility>

#ifdef SANDBOX_HAS_TOMLPP
#include <toml++/toml.hpp>
#endif

namespace shmup {

namespace {

SatelliteConfig::PositionMode parsePositionMode(std::string_view s) {
  if (s == "fixed")
    return SatelliteConfig::PositionMode::Fixed;
  if (s == "orbit")
    return SatelliteConfig::PositionMode::Orbit;
  if (s == "formation")
    return SatelliteConfig::PositionMode::Formation;
  return SatelliteConfig::PositionMode::Fixed;
}

}  // namespace

bool SatelliteConfig::loadFromToml([[maybe_unused]] const char* path) {
#ifndef SANDBOX_HAS_TOMLPP
  std::printf("SatelliteConfig: toml++ not enabled, cannot load %s\n", path);
  return false;
#else
  toml::table tbl;
  try {
    tbl = toml::parse_file(path);
  } catch (const toml::parse_error& err) {
    std::printf("SatelliteConfig: parse error in %s: %s\n", path, err.what());
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

  // Parse position_mode
  if (auto p = tbl["position_mode"].value<std::string>()) {
    positionMode = parsePositionMode(*p);
  }

  // Parse [position] section
  if (auto p = tbl["position"].as_table()) {
    if (auto v = (*p)["fixed_offset_x"].value<double>()) {
      position.fixedOffsetX = static_cast<float>(*v);
    }
    if (auto v = (*p)["fixed_offset_y"].value<double>()) {
      position.fixedOffsetY = static_cast<float>(*v);
    }
    if (auto v = (*p)["orbit_radius"].value<double>()) {
      position.orbitRadius = static_cast<float>(*v);
    }
    if (auto v = (*p)["orbit_speed"].value<double>()) {
      position.orbitSpeed = static_cast<float>(*v);
    }
    if (auto v = (*p)["focused_radius"].value<double>()) {
      position.focusedRadius = static_cast<float>(*v);
    }
    if (auto v = (*p)["focused_offset_x"].value<double>()) {
      position.focusedOffsetX = static_cast<float>(*v);
    }
    if (auto v = (*p)["focused_offset_y"].value<double>()) {
      position.focusedOffsetY = static_cast<float>(*v);
    }
  }

  // Parse [firing] section
  if (auto f = tbl["firing"].as_table()) {
    if (auto v = (*f)["inherit_weapon"].value<bool>()) {
      firing.inheritWeapon = *v;
    }
    if (auto v = (*f)["override_weapon"].value<std::string>()) {
      firing.overrideWeapon = *v;
    }
    if (auto v = (*f)["override_level"].value<int>()) {
      firing.overrideLevel = *v;
    }
    if (auto v = (*f)["fire_angle_offset"].value<double>()) {
      firing.fireAngleOffset = static_cast<float>(*v);
    }
  }

  // Parse [render] section
  if (auto r = tbl["render"].as_table()) {
    if (auto v = (*r)["sprite"].value<std::string>()) {
      render.sprite = *v;
    }
    if (auto v = (*r)["frame_w"].value<int>()) {
      render.frameW = *v;
    }
    if (auto v = (*r)["frame_h"].value<int>()) {
      render.frameH = *v;
    }
    if (auto v = (*r)["frame_count"].value<int>()) {
      render.frameCount = *v;
    }
    if (auto v = (*r)["fps"].value<double>()) {
      render.fps = static_cast<float>(*v);
    }
    if (auto v = (*r)["scale"].value<double>()) {
      render.scale = static_cast<float>(*v);
    }
  }

  return true;
#endif
}

const SatelliteConfig* SatelliteRegistry::get(const std::string& id) {
  auto it = cache_.find(id);
  return (it != cache_.end()) ? &it->second : nullptr;
}

bool SatelliteRegistry::load(const std::string& id, const char* path) {
  SatelliteConfig cfg;
  if (!cfg.loadFromToml(path)) {
    return false;
  }
  cache_[id] = std::move(cfg);
  return true;
}

void SatelliteRegistry::clear() {
  cache_.clear();
}

}  // namespace shmup
