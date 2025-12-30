#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

#include "ecs/Components.h"

class InputScript {
 public:
  bool loadFromToml(const char* path);
  void reset();
  InputState sample(uint64_t frame);

  [[nodiscard]] bool loaded() const { return loaded_; }
  [[nodiscard]] const std::string& path() const { return path_; }

 private:
  struct Held {
    bool left = false;
    bool right = false;
    bool up = false;
    bool down = false;
    bool jump = false;
    bool action1 = false;
    bool action2 = false;
  };

  enum MaskBits : uint32_t {
    kLeft = 1U << 0U,
    kRight = 1U << 1U,
    kUp = 1U << 2U,
    kDown = 1U << 3U,
    kJump = 1U << 4U,
    kAction1 = 1U << 5U,
    kAction2 = 1U << 6U,
  };

  struct Keyframe {
    uint64_t frame = 0;
    uint32_t mask = 0;
    Held values{};
  };

  bool appendFromToml(const std::filesystem::path& path, std::unordered_set<std::string>& seen);

  std::vector<Keyframe> keyframes_;
  Held held_{};
  Held prevHeld_{};
  InputState history_{};
  std::size_t nextIndex_ = 0;
  uint64_t lastFrame_ = 0;
  bool hasLastFrame_ = false;
  bool loaded_ = false;
  std::string path_;
};
