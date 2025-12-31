#include "core/Prefs.h"

#include <toml++/toml.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>

static std::string prefsDirPath() {
  std::string path;
  if (char* prefPath = SDL_GetPrefPath("sdl3-sandbox", "sandbox")) {
    path = std::string(prefPath);
    SDL_free(prefPath);
  }
  return path;
}

static std::string prefsFilePath() {
  const std::string dir = prefsDirPath();
  if (dir.empty())
    return {};
  return dir + "session.toml";
}

bool loadSessionPrefs(SessionPrefs& out) {
  const std::string path = prefsFilePath();
  if (path.empty())
    return false;

  namespace fs = std::filesystem;
  if (!fs::exists(path))
    return false;

  toml::table tbl;
  try {
    tbl = toml::parse_file(path);
  } catch (...) {
    return false;
  }

  if (auto v = tbl.get("stage"))
    out.stagePath = v->value_or(out.stagePath);
  if (auto v = tbl.get("character"))
    out.characterPath = v->value_or(out.characterPath);
  if (auto v = tbl.get("spawn_point"))
    out.spawnPoint = v->value_or(out.spawnPoint);
  if (auto v = tbl.get("spawn_x"))
    out.spawnX = v->value_or(out.spawnX);
  if (auto v = tbl.get("spawn_y"))
    out.spawnY = v->value_or(out.spawnY);
  if (auto v = tbl.get("panels_open"))
    out.panelsOpen = v->value_or(out.panelsOpen);
  if (auto v = tbl.get("auto_reload"))
    out.autoReload = v->value_or(out.autoReload);
  if (auto v = tbl.get("time_scale"))
    out.timeScale = v->value_or(out.timeScale);
  if (auto v = tbl.get("internal_res_mode"))
    out.internalResMode = v->value_or(out.internalResMode);
  if (auto v = tbl.get("integer_scale_only"))
    out.integerScaleOnly = v->value_or(out.integerScaleOnly);
  if (auto v = tbl.get("gamepad_deadzone"))
    out.gamepadDeadzone = v->value_or(out.gamepadDeadzone);

  return true;
}

bool deleteSessionPrefs() {
  const std::string path = prefsFilePath();
  if (path.empty())
    return false;

  namespace fs = std::filesystem;
  std::error_code ec;
  if (!fs::exists(path, ec))
    return true;
  (void)fs::remove(path, ec);
  return !ec;
}

bool deleteImGuiIni() {
  const std::string dir = prefsDirPath();
  if (dir.empty())
    return false;
  const std::string path = dir + "imgui.ini";

  namespace fs = std::filesystem;
  std::error_code ec;
  if (!fs::exists(path, ec))
    return true;
  (void)fs::remove(path, ec);
  return !ec;
}

bool saveSessionPrefs(const SessionPrefs& prefs) {
  const std::string path = prefsFilePath();
  if (path.empty())
    return false;

  toml::table tbl;
  tbl.insert("version", 1);
  tbl.insert("stage", prefs.stagePath);
  tbl.insert("character", prefs.characterPath);
  tbl.insert("spawn_point", prefs.spawnPoint);
  tbl.insert("spawn_x", prefs.spawnX);
  tbl.insert("spawn_y", prefs.spawnY);
  tbl.insert("panels_open", prefs.panelsOpen);
  tbl.insert("auto_reload", prefs.autoReload);
  tbl.insert("time_scale", prefs.timeScale);
  tbl.insert("internal_res_mode", prefs.internalResMode);
  tbl.insert("integer_scale_only", prefs.integerScaleOnly);
  tbl.insert("gamepad_deadzone", prefs.gamepadDeadzone);

  namespace fs = std::filesystem;
  const fs::path outPath(path);
  const fs::path tmpPath = outPath.string() + ".tmp";

  std::ofstream tmp(tmpPath, std::ios::binary | std::ios::trunc);
  if (!tmp.is_open())
    return false;
  tmp << tbl;
  tmp.close();
  if (!tmp)
    return false;

  std::error_code ec;
  fs::rename(tmpPath, outPath, ec);
  if (ec) {
    fs::remove(tmpPath, ec);
    return false;
  }

  return true;
}
