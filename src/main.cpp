#include "core/App.h"

#include <SDL3/SDL_hints.h>

#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace {

void usage(const char* argv0) {
  std::printf(
      "usage: %s [--frames N] [--video-driver NAME] [--sprite PATH] "
      "[--width W] [--height H]\n",
      argv0);
  std::printf("  --frames N           Run N frames then exit (smoke test)\n");
  std::printf("  --video-driver NAME  Force SDL video backend (e.g. x11, wayland, offscreen)\n");
  std::printf("  --sprite PATH        Optional sprite sheet or image to render\n");
  std::printf("  --width W            Window width (default: 1280)\n");
  std::printf("  --height H           Window height (default: 720)\n");
  std::printf("  -h, --help           Show this help\n");
}

bool parseInt(const char* s, int& out) {
  if (!s || !*s)
    return false;
  char* end = nullptr;
  const long v = std::strtol(s, &end, 10);
  if (!end || *end != '\0')
    return false;
  if (v < 1 || v > 100000)
    return false;
  out = static_cast<int>(v);
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  AppConfig cfg{};
  const char* videoDriver = nullptr;

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg(argv[i]);
    if (arg == "--frames") {
      if (i + 1 >= argc || !parseInt(argv[i + 1], cfg.maxFrames)) {
        std::printf("invalid --frames value\n");
        usage(argv[0]);
        return 1;
      }
      ++i;
    } else if (arg == "--video-driver") {
      if (i + 1 >= argc) {
        std::printf("missing --video-driver value\n");
        usage(argv[0]);
        return 1;
      }
      videoDriver = argv[++i];
    } else if (arg == "--sprite") {
      if (i + 1 >= argc) {
        std::printf("missing --sprite value\n");
        usage(argv[0]);
        return 1;
      }
      cfg.spritePath = argv[++i];
    } else if (arg == "--width") {
      if (i + 1 >= argc || !parseInt(argv[i + 1], cfg.width)) {
        std::printf("invalid --width value\n");
        usage(argv[0]);
        return 1;
      }
      ++i;
    } else if (arg == "--height") {
      if (i + 1 >= argc || !parseInt(argv[i + 1], cfg.height)) {
        std::printf("invalid --height value\n");
        usage(argv[0]);
        return 1;
      }
      ++i;
    } else if (arg == "-h" || arg == "--help") {
      usage(argv[0]);
      return 0;
    } else {
      std::printf("unknown option: %s\n", argv[i]);
      usage(argv[0]);
      return 1;
    }
  }

  if (videoDriver) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, videoDriver);
  }

  App app;
  if (!app.init(cfg)) {
    return 1;
  }

  app.run();
  app.shutdown();
  return 0;
}
