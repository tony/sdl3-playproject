#pragma once

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_scancode.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "ecs/Components.h"

struct AppCommands {
  bool quit = false;
  bool respawn = false;
  bool resetState = false;
  bool toggleDebugOverlay = false;
  bool toggleDebugCollision = false;
  bool togglePanels = false;
};

class Input {
 public:
  void init();
  void shutdown();

  void handleEvent(const SDL_Event& e);

  [[nodiscard]] bool hasGamepad() const { return gamepad_ != nullptr; }
  [[nodiscard]] const char* gamepadName() const;
  [[nodiscard]] int gamepadDeadzone() const { return axisDeadzone_; }
  void setGamepadDeadzone(int deadzone);
  void appendLegend(std::vector<std::string>& out) const;

  // Consume edge-triggered flags (pressed/released). Held state is preserved.
  InputState consume();

  // Consume non-gameplay commands (quit/debug toggles).
  AppCommands consumeCommands();

 private:
  void updateDerivedActions();
  void clearEdges();
  void clearCommands();
  void clearGamepadState();
  void tryOpenFirstGamepad();

  std::array<bool, SDL_SCANCODE_COUNT> scancodeDown_{};
  InputState state_{};
  AppCommands commands_{};

  SDL_Gamepad* gamepad_ = nullptr;
  uint32_t gamepadId_ = 0;
  int axisLeftX_ = 0;
  int axisLeftY_ = 0;
  int axisDeadzone_ = 8000;
  bool dpadLeft_ = false;
  bool dpadRight_ = false;
  bool dpadUp_ = false;
  bool dpadDown_ = false;
  bool btnSouth_ = false;  // jump
  bool btnWest_ = false;   // action1
  bool btnEast_ = false;   // action2

  bool f1Held_ = false;
  bool f2Held_ = false;
  bool oneHeld_ = false;
  bool twoHeld_ = false;
  bool hHeld_ = false;
  bool xHeld_ = false;
  bool cHeld_ = false;
};
