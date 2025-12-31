#include "core/InputScript.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <utility>

#include <toml++/toml.h>

#include "util/TomlUtil.h"

namespace {

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

bool InputScript::appendFromToml(const std::filesystem::path& path,
                                 std::unordered_set<std::string>& seen) {
  const std::filesystem::path normalized = path.lexically_normal();
  const std::string pathStr = normalized.string();
  if (pathStr.empty()) {
    return false;
  }

  if (!seen.insert(pathStr).second) {
    TomlUtil::warnf(pathStr.c_str(), "input script include cycle detected; skipping");
    return true;
  }

  toml::table tbl;
  try {
    tbl = toml::parse_file(pathStr);
  } catch (...) {
    return false;
  }

  TomlUtil::warnUnknownKeys(tbl, pathStr.c_str(), "root",
                            {"version", "keyframes", "frames", "include"});

  int version = 1;
  if (auto v = tbl["version"].value<int>())
    version = *v;
  if (version != 1) {
    TomlUtil::warnf(pathStr.c_str(), "input script version {} (expected 1)", version);
  }

  if (auto include = tbl["include"].value<std::string>()) {
    const std::filesystem::path includePath = normalized.parent_path() / *include;
    if (!appendFromToml(includePath, seen)) {
      return false;
    }
  } else if (tbl.contains("include")) {
    TomlUtil::warnf(pathStr.c_str(), "include must be a string path");
  }

  const toml::array* framesArr = nullptr;
  if (auto arr = tbl["keyframes"].as_array())
    framesArr = arr;
  else if (auto arr = tbl["frames"].as_array())
    framesArr = arr;

  if (framesArr) {
    std::size_t idx = 0;
    for (const auto& node : *framesArr) {
      auto t = node.as_table();
      ++idx;
      if (!t)
        continue;

      const std::string scope = "keyframes[" + std::to_string(idx - 1) + "]";
      TomlUtil::warnUnknownKeys(
          *t, pathStr.c_str(), scope,
          {"frame", "at", "left", "right", "up", "down", "jump", "action1", "action2"});

      uint64_t frame = 0;
      bool hasFrame = false;
      if (auto v = t->get("frame")) {
        const int f = v->value_or(-1);
        if (f >= 0) {
          frame = static_cast<uint64_t>(f);
          hasFrame = true;
        }
      }
      if (!hasFrame) {
        if (auto v = t->get("at")) {
          const int f = v->value_or(-1);
          if (f >= 0) {
            frame = static_cast<uint64_t>(f);
            hasFrame = true;
          }
        }
      }

      if (!hasFrame) {
        TomlUtil::warnf(pathStr.c_str(), "{} missing frame (use frame = N)", scope);
        continue;
      }

      Keyframe kf{};
      kf.frame = frame;

      if (auto v = t->get("left")) {
        kf.mask |= kLeft;
        kf.values.left = v->value_or(kf.values.left);
      }
      if (auto v = t->get("right")) {
        kf.mask |= kRight;
        kf.values.right = v->value_or(kf.values.right);
      }
      if (auto v = t->get("up")) {
        kf.mask |= kUp;
        kf.values.up = v->value_or(kf.values.up);
      }
      if (auto v = t->get("down")) {
        kf.mask |= kDown;
        kf.values.down = v->value_or(kf.values.down);
      }
      if (auto v = t->get("jump")) {
        kf.mask |= kJump;
        kf.values.jump = v->value_or(kf.values.jump);
      }
      if (auto v = t->get("action1")) {
        kf.mask |= kAction1;
        kf.values.action1 = v->value_or(kf.values.action1);
      }
      if (auto v = t->get("action2")) {
        kf.mask |= kAction2;
        kf.values.action2 = v->value_or(kf.values.action2);
      }

      keyframes_.push_back(kf);
    }
  }

  return true;
}

// NOLINTNEXTLINE
bool InputScript::loadFromToml(const char* path) {
  keyframes_.clear();
  held_ = Held{};
  prevHeld_ = Held{};
  nextIndex_ = 0;
  lastFrame_ = 0;
  hasLastFrame_ = false;
  loaded_ = false;
  path_ = path ? path : "";

  std::unordered_set<std::string> seen;
  if (!appendFromToml(path_, seen)) {
    return false;
  }

  std::stable_sort(keyframes_.begin(), keyframes_.end(),
                   [](const Keyframe& a, const Keyframe& b) { return a.frame < b.frame; });

  loaded_ = true;
  return true;
}

void InputScript::reset() {
  held_ = Held{};
  prevHeld_ = Held{};
  history_ = InputState{};
  nextIndex_ = 0;
  lastFrame_ = 0;
  hasLastFrame_ = false;
}

// NOLINTNEXTLINE
InputState InputScript::sample(uint64_t frame) {
  if (!loaded_)
    return InputState{};

  if (!hasLastFrame_ || frame < lastFrame_) {
    reset();
  }

  while (nextIndex_ < keyframes_.size() && keyframes_[nextIndex_].frame <= frame) {
    const Keyframe& kf = keyframes_[nextIndex_];
    if ((kf.mask & kLeft) != 0U)
      held_.left = kf.values.left;
    if ((kf.mask & kRight) != 0U)
      held_.right = kf.values.right;
    if ((kf.mask & kUp) != 0U)
      held_.up = kf.values.up;
    if ((kf.mask & kDown) != 0U)
      held_.down = kf.values.down;
    if ((kf.mask & kJump) != 0U)
      held_.jump = kf.values.jump;
    if ((kf.mask & kAction1) != 0U)
      held_.action1 = kf.values.action1;
    if ((kf.mask & kAction2) != 0U)
      held_.action2 = kf.values.action2;
    ++nextIndex_;
  }

  InputState out{};
  out.left = held_.left;
  out.right = held_.right;

  out.upHeld = held_.up;
  out.upPressed = held_.up && !prevHeld_.up;
  out.upReleased = !held_.up && prevHeld_.up;

  out.downHeld = held_.down;
  out.downPressed = held_.down && !prevHeld_.down;
  out.downReleased = !held_.down && prevHeld_.down;

  out.jumpHeld = held_.jump;
  out.jumpPressed = held_.jump && !prevHeld_.jump;
  out.jumpReleased = !held_.jump && prevHeld_.jump;

  out.action1Held = held_.action1;
  out.action1Pressed = held_.action1 && !prevHeld_.action1;
  out.action1Released = !held_.action1 && prevHeld_.action1;

  out.action2Held = held_.action2;
  out.action2Pressed = held_.action2 && !prevHeld_.action2;
  out.action2Released = !held_.action2 && prevHeld_.action2;

  out.upHeldFrames = history_.upHeldFrames;
  out.upPressedFrames = history_.upPressedFrames;
  out.downHeldFrames = history_.downHeldFrames;
  out.downPressedFrames = history_.downPressedFrames;
  out.jumpHeldFrames = history_.jumpHeldFrames;
  out.jumpPressedFrames = history_.jumpPressedFrames;
  out.action1HeldFrames = history_.action1HeldFrames;
  out.action1PressedFrames = history_.action1PressedFrames;
  out.action2HeldFrames = history_.action2HeldFrames;
  out.action2PressedFrames = history_.action2PressedFrames;
  updateInputHistory(out);

  history_ = out;
  prevHeld_ = held_;
  lastFrame_ = frame;
  hasLastFrame_ = true;
  return out;
}
