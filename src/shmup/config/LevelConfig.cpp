#include "shmup/config/LevelConfig.h"

#include <cstdio>
#include <filesystem>
#include <unordered_set>
#include <utility>

#ifdef SANDBOX_HAS_TOMLPP
#include <toml++/toml.hpp>
#endif

namespace shmup {

namespace {

LevelConfig::Formation parseFormation(const std::string& str) {
  if (str == "single")
    return LevelConfig::Formation::Single;
  if (str == "line")
    return LevelConfig::Formation::Line;
  if (str == "column")
    return LevelConfig::Formation::Column;
  if (str == "v_shape")
    return LevelConfig::Formation::VShape;
  if (str == "inverse_v")
    return LevelConfig::Formation::InverseV;
  if (str == "diagonal")
    return LevelConfig::Formation::Diagonal;
  if (str == "random")
    return LevelConfig::Formation::Random;
  return LevelConfig::Formation::Single;
}

}  // namespace

bool LevelConfig::loadFromToml([[maybe_unused]] const char* path) {
#ifndef SANDBOX_HAS_TOMLPP
  std::printf("LevelConfig: toml++ not enabled, cannot load %s\n", path);
  return false;
#else
  std::unordered_set<std::string> seen;

  auto appendFromToml = [&](auto&& self, const std::filesystem::path& filePath) -> bool {
    const std::filesystem::path normalized = filePath.lexically_normal();
    const std::string pathStr = normalized.string();

    if (!seen.insert(pathStr).second) {
      std::printf("LevelConfig: include cycle detected: %s\n", pathStr.c_str());
      return true;
    }

    toml::table tbl;
    try {
      tbl = toml::parse_file(pathStr);
    } catch (const toml::parse_error& err) {
      std::printf("LevelConfig: parse error in %s: %s\n", pathStr.c_str(), err.what());
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

    // Parse [level] section
    if (auto l = tbl["level"].as_table()) {
      if (auto val = (*l)["id"].value<std::string>()) {
        id = *val;
      }
      if (auto val = (*l)["name"].value<std::string>()) {
        displayName = *val;
      }
      if (auto val = (*l)["scroll_speed"].value<double>()) {
        properties.scrollSpeed = static_cast<float>(*val);
      }
      if (auto val = (*l)["duration_seconds"].value<double>()) {
        properties.duration = static_cast<float>(*val);
      }
      if (auto val = (*l)["boss_position"].value<double>()) {
        properties.bossPosition = static_cast<float>(*val);
      }
    }

    // Parse [[backgrounds]] array
    if (auto bgs = tbl["backgrounds"].as_array()) {
      for (const auto& bg : *bgs) {
        if (auto t = bg.as_table()) {
          Background b;
          if (auto val = (*t)["sprite"].value<std::string>()) {
            b.spritePath = *val;
          }
          if (auto val = (*t)["scroll_speed_ratio"].value<double>()) {
            b.scrollSpeedRatio = static_cast<float>(*val);
          }
          if (auto val = (*t)["repeat_x"].value<bool>()) {
            b.repeatX = *val;
          }
          if (auto val = (*t)["repeat_y"].value<bool>()) {
            b.repeatY = *val;
          }
          if (auto val = (*t)["offset_y"].value<double>()) {
            b.offsetY = static_cast<float>(*val);
          }
          if (auto val = (*t)["layer"].value<int>()) {
            b.layer = *val;
          }
          backgrounds.push_back(b);
        }
      }
    }

    // Parse [[waves]] array
    if (auto ws = tbl["waves"].as_array()) {
      for (const auto& w : *ws) {
        if (auto t = w.as_table()) {
          Wave wave;
          if (auto val = (*t)["id"].value<std::string>()) {
            wave.id = *val;
          }
          if (auto val = (*t)["enemy_type"].value<std::string>()) {
            wave.enemyType = *val;
          }
          if (auto val = (*t)["formation"].value<std::string>()) {
            wave.formation = parseFormation(*val);
          }
          if (auto val = (*t)["count"].value<int>()) {
            wave.count = *val;
          }
          if (auto val = (*t)["trigger_position"].value<double>()) {
            wave.triggerPosition = static_cast<float>(*val);
          }
          if (auto val = (*t)["spawn_x"].value<double>()) {
            wave.spawnX = static_cast<float>(*val);
          }
          if (auto val = (*t)["spawn_y"].value<double>()) {
            wave.spawnY = static_cast<float>(*val);
          }
          if (auto val = (*t)["spacing"].value<double>()) {
            wave.spacing = static_cast<float>(*val);
          }
          if (auto val = (*t)["delay_between"].value<double>()) {
            wave.delayBetween = static_cast<float>(*val);
          }
          waves.push_back(wave);
        }
      }
    }

    // Parse [[sections]] or [[boss_sections]] array
    if (auto bs = tbl["boss_sections"].as_array()) {
      for (const auto& b : *bs) {
        if (auto t = b.as_table()) {
          BossSection boss;
          if (auto val = (*t)["boss_id"].value<std::string>()) {
            boss.bossId = *val;
          }
          if (auto val = (*t)["trigger_position"].value<double>()) {
            boss.triggerPosition = static_cast<float>(*val);
          }
          if (auto val = (*t)["pause_scroll"].value<bool>()) {
            boss.pauseScroll = *val;
          }
          bossSections.push_back(boss);
        }
      }
    }

    return true;
  };

  return appendFromToml(appendFromToml, path);
#endif
}

const LevelConfig* LevelRegistry::get(const std::string& id) {
  auto it = cache_.find(id);
  return (it != cache_.end()) ? &it->second : nullptr;
}

bool LevelRegistry::load(const std::string& id, const char* path) {
  LevelConfig cfg;
  if (!cfg.loadFromToml(path)) {
    return false;
  }
  cache_[id] = std::move(cfg);
  return true;
}

void LevelRegistry::clear() {
  cache_.clear();
}

}  // namespace shmup
