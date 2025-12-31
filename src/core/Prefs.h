#pragma once

#include <string>

struct SessionPrefs {
  std::string stagePath;
  std::string characterPath;
  std::string spawnPoint;
  float spawnX = 120.0F;
  float spawnY = 120.0F;
  bool panelsOpen = true;
  bool autoReload = false;
  float timeScale = 1.0F;
  int internalResMode = 0;  // 0=auto, then fixed modes in App
  bool integerScaleOnly = false;
  int gamepadDeadzone = 8000;
};

bool loadSessionPrefs(SessionPrefs& out);
bool saveSessionPrefs(const SessionPrefs& prefs);
bool deleteSessionPrefs();
bool deleteImGuiIni();
