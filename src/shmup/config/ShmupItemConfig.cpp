#include "shmup/config/ShmupItemConfig.h"

#include <cstdio>
#include <filesystem>
#include <unordered_set>
#include <utility>

#ifdef SANDBOX_HAS_TOMLPP
#include <toml++/toml.hpp>
#endif

namespace shmup {

namespace {

ShmupItemConfig::EffectType parseEffectType(const std::string& str) {
  if (str == "weapon_upgrade")
    return ShmupItemConfig::EffectType::WeaponUpgrade;
  if (str == "life")
    return ShmupItemConfig::EffectType::Life;
  if (str == "bomb")
    return ShmupItemConfig::EffectType::Bomb;
  if (str == "score_bonus" || str == "score")
    return ShmupItemConfig::EffectType::ScoreBonus;
  if (str == "shield")
    return ShmupItemConfig::EffectType::Shield;
  if (str == "full_power")
    return ShmupItemConfig::EffectType::FullPower;
  return ShmupItemConfig::EffectType::ScoreBonus;
}

ShmupItemConfig::MovementType parseMovementType(const std::string& str) {
  if (str == "float")
    return ShmupItemConfig::MovementType::Float;
  if (str == "bounce")
    return ShmupItemConfig::MovementType::Bounce;
  if (str == "magnet")
    return ShmupItemConfig::MovementType::Magnet;
  if (str == "stationary")
    return ShmupItemConfig::MovementType::Stationary;
  return ShmupItemConfig::MovementType::Float;
}

}  // namespace

bool ShmupItemConfig::loadFromToml([[maybe_unused]] const char* path) {
#ifndef SANDBOX_HAS_TOMLPP
  std::printf("ShmupItemConfig: toml++ not enabled, cannot load %s\n", path);
  return false;
#else
  std::unordered_set<std::string> seen;

  auto appendFromToml = [&](auto&& self, const std::filesystem::path& filePath) -> bool {
    const std::filesystem::path normalized = filePath.lexically_normal();
    const std::string pathStr = normalized.string();

    if (!seen.insert(pathStr).second) {
      std::printf("ShmupItemConfig: include cycle detected: %s\n", pathStr.c_str());
      return true;
    }

    toml::table tbl;
    try {
      tbl = toml::parse_file(pathStr);
    } catch (const toml::parse_error& err) {
      std::printf("ShmupItemConfig: parse error in %s: %s\n", pathStr.c_str(), err.what());
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

    // Parse [item] section
    if (auto i = tbl["item"].as_table()) {
      if (auto val = (*i)["id"].value<std::string>()) {
        id = *val;
      }
      if (auto val = (*i)["display"].value<std::string>()) {
        displayName = *val;
      }
    }

    // Parse [effect] section
    if (auto e = tbl["effect"].as_table()) {
      if (auto val = (*e)["type"].value<std::string>()) {
        effect.type = parseEffectType(*val);
      }
      if (auto val = (*e)["value"].value<int>()) {
        effect.value = *val;
      }
    }

    // Parse [pickup] section
    if (auto p = tbl["pickup"].as_table()) {
      if (auto val = (*p)["radius"].value<double>()) {
        pickup.radius = static_cast<float>(*val);
      }
      if (auto val = (*p)["lifetime_frames"].value<int>()) {
        pickup.lifetimeFrames = *val;
      }
      if (auto val = (*p)["magnet_range"].value<double>()) {
        pickup.magnetRange = static_cast<float>(*val);
      }
      if (auto val = (*p)["magnet_speed"].value<double>()) {
        pickup.magnetSpeed = static_cast<float>(*val);
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
      if (auto val = (*m)["bounce_speed"].value<double>()) {
        movement.bounceSpeed = static_cast<float>(*val);
      }
    }

    // Parse [render] section
    if (auto r = tbl["render"].as_table()) {
      if (auto val = (*r)["sprite"].value<std::string>()) {
        render.sprite = *val;
      }
      if (auto val = (*r)["frame_w"].value<int>()) {
        render.frameW = *val;
      }
      if (auto val = (*r)["frame_h"].value<int>()) {
        render.frameH = *val;
      }
      if (auto val = (*r)["frames"].value<int>()) {
        render.frames = *val;
      }
      if (auto val = (*r)["fps"].value<double>()) {
        render.fps = static_cast<float>(*val);
      }
      if (auto val = (*r)["scale"].value<double>()) {
        render.scale = static_cast<float>(*val);
      }
    }

    return true;
  };

  return appendFromToml(appendFromToml, path);
#endif
}

const ShmupItemConfig* ShmupItemRegistry::get(const std::string& id) {
  auto it = cache_.find(id);
  return (it != cache_.end()) ? &it->second : nullptr;
}

bool ShmupItemRegistry::load(const std::string& id, const char* path) {
  ShmupItemConfig cfg;
  if (!cfg.loadFromToml(path)) {
    return false;
  }
  cache_[id] = std::move(cfg);
  return true;
}

void ShmupItemRegistry::clear() {
  cache_.clear();
}

}  // namespace shmup
