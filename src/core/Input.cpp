#include "core/Input.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_joystick.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_stdinc.h>

#include <memory>

namespace {

// SDL key mappings live here and are consumed by both input processing and UI legends.
constexpr SDL_Scancode kMoveLeftPrimary = SDL_SCANCODE_LEFT;
constexpr SDL_Scancode kMoveLeftAlt = SDL_SCANCODE_A;
constexpr SDL_Scancode kMoveRightPrimary = SDL_SCANCODE_RIGHT;
constexpr SDL_Scancode kMoveRightAlt = SDL_SCANCODE_D;

constexpr SDL_Scancode kUpPrimary = SDL_SCANCODE_UP;
constexpr SDL_Scancode kUpAlt = SDL_SCANCODE_W;

constexpr SDL_Scancode kDownPrimary = SDL_SCANCODE_DOWN;
constexpr SDL_Scancode kDownAlt = SDL_SCANCODE_S;

constexpr SDL_Scancode kJumpKey = SDL_SCANCODE_J;
constexpr SDL_Scancode kAction1Key1 = SDL_SCANCODE_L;
constexpr SDL_Scancode kAction1Key2 = SDL_SCANCODE_I;
constexpr SDL_Scancode kAction2Key1 = SDL_SCANCODE_K;
constexpr SDL_Scancode kAction2Key2 = SDL_SCANCODE_SEMICOLON;

constexpr SDL_Scancode kRespawnKey = SDL_SCANCODE_X;
constexpr SDL_Scancode kResetKey = SDL_SCANCODE_C;

constexpr SDL_Scancode kCtrlLeft = SDL_SCANCODE_LCTRL;
constexpr SDL_Scancode kCtrlRight = SDL_SCANCODE_RCTRL;
constexpr SDL_Scancode kTogglePanelsKey = SDL_SCANCODE_H;

constexpr SDL_Scancode kToggleOverlayKey = SDL_SCANCODE_1;
constexpr SDL_Scancode kToggleCollisionKey = SDL_SCANCODE_2;

constexpr SDL_Scancode kToggleOverlayAltKey = SDL_SCANCODE_F1;
constexpr SDL_Scancode kToggleCollisionAltKey = SDL_SCANCODE_F2;

constexpr SDL_GamepadAxis kMoveAxisX = SDL_GAMEPAD_AXIS_LEFTX;
constexpr SDL_GamepadAxis kMoveAxisY = SDL_GAMEPAD_AXIS_LEFTY;
constexpr SDL_GamepadButton kDpadLeftButton = SDL_GAMEPAD_BUTTON_DPAD_LEFT;
constexpr SDL_GamepadButton kDpadRightButton = SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
constexpr SDL_GamepadButton kDpadUpButton = SDL_GAMEPAD_BUTTON_DPAD_UP;
constexpr SDL_GamepadButton kDpadDownButton = SDL_GAMEPAD_BUTTON_DPAD_DOWN;
constexpr SDL_GamepadButton kJumpButton = SDL_GAMEPAD_BUTTON_SOUTH;
constexpr SDL_GamepadButton kAction1Button = SDL_GAMEPAD_BUTTON_WEST;
constexpr SDL_GamepadButton kAction2Button = SDL_GAMEPAD_BUTTON_EAST;

const char* prettyScancode(SDL_Scancode sc) {
  switch (sc) {
    case SDL_SCANCODE_LEFT:
      return "\u2190";
    case SDL_SCANCODE_RIGHT:
      return "\u2192";
    case SDL_SCANCODE_UP:
      return "\u2191";
    case SDL_SCANCODE_DOWN:
      return "\u2193";
    case SDL_SCANCODE_SEMICOLON:
      return ";";
    default:
      break;
  }

  const char* name = SDL_GetScancodeName(sc);
  if (!name || !*name)
    return "?";
  return name;
}

void updateHeldFrames(bool held, bool pressed, int& frames) {
  if (!held) {
    frames = 0;
    return;
  }
  if (pressed) {
    frames = 1;
    return;
  }
  if (frames < InputState::kUnpressedFrames) {
    ++frames;
  }
}

void updatePressedFrames(bool pressed, int& frames) {
  if (pressed) {
    frames = 0;
    return;
  }
  if (frames < InputState::kUnpressedFrames) {
    ++frames;
  }
}

void updateInputHistory(InputState& state) {
  updateHeldFrames(state.upHeld, state.upPressed, state.upHeldFrames);
  updatePressedFrames(state.upPressed, state.upPressedFrames);
  updateHeldFrames(state.downHeld, state.downPressed, state.downHeldFrames);
  updatePressedFrames(state.downPressed, state.downPressedFrames);
  updateHeldFrames(state.jumpHeld, state.jumpPressed, state.jumpHeldFrames);
  updatePressedFrames(state.jumpPressed, state.jumpPressedFrames);
  updateHeldFrames(state.action1Held, state.action1Pressed, state.action1HeldFrames);
  updatePressedFrames(state.action1Pressed, state.action1PressedFrames);
  updateHeldFrames(state.action2Held, state.action2Pressed, state.action2HeldFrames);
  updatePressedFrames(state.action2Pressed, state.action2PressedFrames);
}

}  // namespace

void Input::clearGamepadState() {
  axisLeftX_ = 0;
  axisLeftY_ = 0;
  dpadLeft_ = false;
  dpadRight_ = false;
  dpadUp_ = false;
  dpadDown_ = false;
  btnSouth_ = false;
  btnWest_ = false;
  btnEast_ = false;
}

void Input::tryOpenFirstGamepad() {
  if (gamepad_)
    return;

  int count = 0;
  using GamepadListPtr = std::unique_ptr<SDL_JoystickID, decltype(&SDL_free)>;
  GamepadListPtr ids(SDL_GetGamepads(&count), SDL_free);
  if (!ids || count <= 0) {
    return;
  }

  for (int i = 0; i < count; ++i) {
    SDL_Gamepad* gp = SDL_OpenGamepad(ids.get()[i]);
    if (!gp)
      continue;
    gamepad_ = gp;
    gamepadId_ = ids.get()[i];
    break;
  }
}

void Input::init() {
  SDL_SetGamepadEventsEnabled(true);
  tryOpenFirstGamepad();
}

void Input::shutdown() {
  if (gamepad_) {
    SDL_CloseGamepad(gamepad_);
    gamepad_ = nullptr;
  }
  gamepadId_ = 0;
  clearGamepadState();
}

// NOLINTNEXTLINE
void Input::handleEvent(const SDL_Event& e) {
  if (e.type == SDL_EVENT_GAMEPAD_ADDED) {
    if (!gamepad_) {
      SDL_Gamepad* gp = SDL_OpenGamepad(e.gdevice.which);
      if (gp) {
        gamepad_ = gp;
        gamepadId_ = e.gdevice.which;
      }
    }
    return;
  }
  if (e.type == SDL_EVENT_GAMEPAD_REMOVED) {
    if (gamepad_ && e.gdevice.which == gamepadId_) {
      SDL_CloseGamepad(gamepad_);
      gamepad_ = nullptr;
      gamepadId_ = 0;
      clearGamepadState();
      tryOpenFirstGamepad();
      updateDerivedActions();
    }
    return;
  }
  if (e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
    if (gamepad_ && e.gaxis.which == gamepadId_) {
      if (e.gaxis.axis == kMoveAxisX)
        axisLeftX_ = static_cast<int>(e.gaxis.value);
      else if (e.gaxis.axis == kMoveAxisY)
        axisLeftY_ = static_cast<int>(e.gaxis.value);
      updateDerivedActions();
    }
    return;
  }
  if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || e.type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
    if (gamepad_ && e.gbutton.which == gamepadId_) {
      const bool down = e.gbutton.down;
      switch (e.gbutton.button) {
        case kDpadLeftButton:
          dpadLeft_ = down;
          break;
        case kDpadRightButton:
          dpadRight_ = down;
          break;
        case kDpadUpButton:
          dpadUp_ = down;
          break;
        case kDpadDownButton:
          dpadDown_ = down;
          break;
        case kJumpButton:
          btnSouth_ = down;
          break;
        case kAction1Button:
          btnWest_ = down;
          break;
        case kAction2Button:
          btnEast_ = down;
          break;
        default:
          break;
      }
      updateDerivedActions();
    }
    return;
  }

  if (e.type != SDL_EVENT_KEY_DOWN && e.type != SDL_EVENT_KEY_UP)
    return;

  const int sc = static_cast<int>(e.key.scancode);
  if (sc < 0 || sc >= static_cast<int>(scancodeDown_.size()))
    return;

  scancodeDown_[sc] = e.key.down;
  updateDerivedActions();
}

void Input::setGamepadDeadzone(int deadzone) {
  if (deadzone < 0)
    deadzone = 0;
  if (deadzone > 32767)
    deadzone = 32767;
  axisDeadzone_ = deadzone;
  updateDerivedActions();
}

const char* Input::gamepadName() const {
  if (!gamepad_)
    return nullptr;
  const char* name = SDL_GetGamepadName(gamepad_);
  if (!name || !*name)
    return nullptr;
  return name;
}

void Input::appendLegend(std::vector<std::string>& out) const {
  out.push_back(std::string("Move: ") + prettyScancode(kMoveLeftPrimary) + "/" +
                prettyScancode(kMoveRightPrimary) + " or " + prettyScancode(kMoveLeftAlt) + "/" +
                prettyScancode(kMoveRightAlt) + "  (pad: D-pad / left stick)");
  out.push_back(std::string("Up: ") + prettyScancode(kUpPrimary) + " or " + prettyScancode(kUpAlt) +
                "  (pad: D-pad up / left stick)");
  out.push_back(std::string("Down: ") + prettyScancode(kDownPrimary) + " or " +
                prettyScancode(kDownAlt) + "  (pad: D-pad down / left stick)");
  out.push_back(std::string("Jump: ") + prettyScancode(kJumpKey) + "  (pad: South)");
  out.emplace_back("Drop-through: Down+Jump (one-way)");
  out.push_back(std::string("Action1: ") + prettyScancode(kAction1Key1) + "/" +
                prettyScancode(kAction1Key2) + "  (pad: West)");
  out.push_back(std::string("Action2: ") + prettyScancode(kAction2Key1) + "/" +
                prettyScancode(kAction2Key2) + "  (pad: East)");
  if (const char* gpName = gamepadName())
    out.push_back(std::string("Gamepad: ") + gpName);

  out.push_back(std::string("Respawn: (keyboard) ") + prettyScancode(kRespawnKey) +
                "  Reset: (keyboard) " + prettyScancode(kResetKey));
  out.push_back(std::string("UI: Ctrl-") + prettyScancode(kTogglePanelsKey) +
                " hide/show  Quit: Ctrl-" + prettyScancode(kResetKey));
  out.push_back(std::string("Toggles: Ctrl-") + prettyScancode(kToggleOverlayKey) +
                " overlay  Ctrl-" + prettyScancode(kToggleCollisionKey) + " collision  (" +
                prettyScancode(kToggleOverlayAltKey) + "/" +
                prettyScancode(kToggleCollisionAltKey) + ")");
}

InputState Input::consume() {
  updateInputHistory(state_);
  InputState out = state_;
  clearEdges();
  return out;
}

AppCommands Input::consumeCommands() {
  AppCommands out = commands_;
  clearCommands();
  return out;
}

void Input::clearEdges() {
  state_.upPressed = false;
  state_.upReleased = false;
  state_.downPressed = false;
  state_.downReleased = false;
  state_.jumpPressed = false;
  state_.jumpReleased = false;
  state_.action1Pressed = false;
  state_.action1Released = false;
  state_.action2Pressed = false;
  state_.action2Released = false;
}

void Input::clearCommands() {
  commands_.quit = false;
  commands_.respawn = false;
  commands_.resetState = false;
  commands_.toggleDebugOverlay = false;
  commands_.toggleDebugCollision = false;
  commands_.togglePanels = false;
}

// NOLINTNEXTLINE
void Input::updateDerivedActions() {
  const bool leftKey = scancodeDown_[kMoveLeftPrimary] || scancodeDown_[kMoveLeftAlt];
  const bool rightKey = scancodeDown_[kMoveRightPrimary] || scancodeDown_[kMoveRightAlt];

  const bool leftPad = dpadLeft_ || (axisLeftX_ < -axisDeadzone_);
  const bool rightPad = dpadRight_ || (axisLeftX_ > axisDeadzone_);

  state_.left = leftKey || leftPad;
  state_.right = rightKey || rightPad;

  const bool ctrlHeld = scancodeDown_[kCtrlLeft] || scancodeDown_[kCtrlRight];

  const bool upKey = scancodeDown_[kUpPrimary] || scancodeDown_[kUpAlt];
  const bool upPad = dpadUp_ || (axisLeftY_ < -axisDeadzone_);
  const bool upNow = upKey || upPad;
  if (upNow && !state_.upHeld)
    state_.upPressed = true;
  if (!upNow && state_.upHeld)
    state_.upReleased = true;
  state_.upHeld = upNow;

  const bool downKey = scancodeDown_[kDownPrimary] || scancodeDown_[kDownAlt];
  const bool downPad = dpadDown_ || (axisLeftY_ > axisDeadzone_);
  const bool downNow = downKey || downPad;
  if (downNow && !state_.downHeld)
    state_.downPressed = true;
  if (!downNow && state_.downHeld)
    state_.downReleased = true;
  state_.downHeld = downNow;

  const bool jumpNow = scancodeDown_[kJumpKey] || btnSouth_;
  if (jumpNow && !state_.jumpHeld)
    state_.jumpPressed = true;
  if (!jumpNow && state_.jumpHeld)
    state_.jumpReleased = true;
  state_.jumpHeld = jumpNow;

  const bool action1Key = scancodeDown_[kAction1Key1] || scancodeDown_[kAction1Key2];
  const bool action1Now = btnWest_ || action1Key;
  if (action1Now && !state_.action1Held)
    state_.action1Pressed = true;
  if (!action1Now && state_.action1Held)
    state_.action1Released = true;
  state_.action1Held = action1Now;

  const bool action2Key = scancodeDown_[kAction2Key1] || scancodeDown_[kAction2Key2];
  const bool action2Now = btnEast_ || action2Key;
  if (action2Now && !state_.action2Held)
    state_.action2Pressed = true;
  if (!action2Now && state_.action2Held)
    state_.action2Released = true;
  state_.action2Held = action2Now;

  const bool cNow = scancodeDown_[kResetKey];
  if (cNow && !cHeld_) {
    if (ctrlHeld)
      commands_.quit = true;  // Ctrl-C
    else
      commands_.resetState = true;
  }
  cHeld_ = cNow;

  const bool xNow = scancodeDown_[kRespawnKey];
  if (xNow && !xHeld_)
    commands_.respawn = true;
  xHeld_ = xNow;

  const bool oneNow = scancodeDown_[kToggleOverlayKey];
  if (oneNow && !oneHeld_ && ctrlHeld)
    commands_.toggleDebugOverlay = true;  // Ctrl-1
  oneHeld_ = oneNow;

  const bool twoNow = scancodeDown_[kToggleCollisionKey];
  if (twoNow && !twoHeld_ && ctrlHeld)
    commands_.toggleDebugCollision = true;  // Ctrl-2
  twoHeld_ = twoNow;

  const bool hNow = scancodeDown_[kTogglePanelsKey];
  if (hNow && !hHeld_ && ctrlHeld)
    commands_.togglePanels = true;  // Ctrl-H
  hHeld_ = hNow;

  const bool f1Now = scancodeDown_[kToggleOverlayAltKey];
  if (f1Now && !f1Held_)
    commands_.toggleDebugOverlay = true;
  f1Held_ = f1Now;

  const bool f2Now = scancodeDown_[kToggleCollisionAltKey];
  if (f2Now && !f2Held_)
    commands_.toggleDebugCollision = true;
  f2Held_ = f2Now;
}
