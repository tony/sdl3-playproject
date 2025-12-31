#pragma once

#include <SDL3/SDL.h>

#include <cstdint>

#include "core/SpriteCache.h"

struct AppConfig {
  const char* title = "SDL3 Sandbox";
  int width = 1280;
  int height = 720;
  int maxFrames = -1;  // -1 = run until quit
  const char* spritePath = nullptr;
};

class App {
 public:
  bool init(const AppConfig& cfg);
  void run();
  void shutdown();

 private:
  void handleEvent(const SDL_Event& e);
  void update(float dt);
  void render();

  AppConfig cfg_{};
  SDL_Window* window_ = nullptr;
  SDL_Renderer* renderer_ = nullptr;
  bool running_ = true;

  bool leftHeld_ = false;
  bool rightHeld_ = false;
  bool upHeld_ = false;
  bool downHeld_ = false;

  float posX_ = 0.0F;
  float posY_ = 0.0F;

  SpriteCache sprites_{};
  SDL_Texture* sprite_ = nullptr;
  float spriteW_ = 0.0F;
  float spriteH_ = 0.0F;
};
