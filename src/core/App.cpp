#include "core/App.h"

#include <algorithm>
#include <cstdio>

namespace {

float clampf(float v, float lo, float hi) {
  return std::min(std::max(v, lo), hi);
}

}  // namespace

bool App::init(const AppConfig& cfg) {
  cfg_ = cfg;

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::printf("SDL_Init failed: %s\n", SDL_GetError());
    return false;
  }

  window_ = SDL_CreateWindow(cfg_.title, cfg_.width, cfg_.height, SDL_WINDOW_RESIZABLE);
  if (!window_) {
    std::printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
    return false;
  }

  renderer_ = SDL_CreateRenderer(window_, nullptr);
  if (renderer_) {
    SDL_SetRenderVSync(renderer_, 1);
  } else {
    std::printf("SDL_CreateRenderer failed: %s\n", SDL_GetError());
    return false;
  }

  sprites_.init(renderer_);
  if (cfg_.spritePath) {
    sprite_ = sprites_.get(cfg_.spritePath);
    if (sprite_) {
      float w = 0;
      float h = 0;
      if (SDL_GetTextureSize(sprite_, &w, &h)) {
        spriteW_ = w;
        spriteH_ = h;
      }
    } else {
      std::printf("Sprite load failed: %s\n", cfg_.spritePath);
    }
  }

  posX_ = static_cast<float>(cfg_.width) * 0.5F;
  posY_ = static_cast<float>(cfg_.height) * 0.5F;

  return true;
}

void App::run() {
  uint64_t lastTicks = SDL_GetTicks();
  int frames = 0;

  while (running_) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      handleEvent(e);
    }

    const uint64_t now = SDL_GetTicks();
    float dt = static_cast<float>(now - lastTicks) / 1000.0F;
    if (dt > 0.25F) {
      dt = 0.25F;
    }
    lastTicks = now;

    update(dt);
    render();

    if (cfg_.maxFrames > 0) {
      ++frames;
      if (frames >= cfg_.maxFrames) {
        running_ = false;
      }
    }
  }
}

void App::shutdown() {
  sprites_.shutdown();

  if (renderer_) {
    SDL_DestroyRenderer(renderer_);
    renderer_ = nullptr;
  }
  if (window_) {
    SDL_DestroyWindow(window_);
    window_ = nullptr;
  }

  SDL_Quit();
}

void App::handleEvent(const SDL_Event& e) {
  if (e.type == SDL_EVENT_QUIT) {
    running_ = false;
    return;
  }

  if (e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP) {
    const bool down = e.key.down;
    switch (e.key.scancode) {
      case SDL_SCANCODE_LEFT:
        leftHeld_ = down;
        break;
      case SDL_SCANCODE_RIGHT:
        rightHeld_ = down;
        break;
      case SDL_SCANCODE_UP:
        upHeld_ = down;
        break;
      case SDL_SCANCODE_DOWN:
        downHeld_ = down;
        break;
      case SDL_SCANCODE_ESCAPE:
        if (down)
          running_ = false;
        break;
      default:
        break;
    }
  }
}

void App::update(float dt) {
  int winW = cfg_.width;
  int winH = cfg_.height;
  if (window_) {
    SDL_GetWindowSize(window_, &winW, &winH);
  }

  const float speed = 300.0F;
  const float dx = (rightHeld_ ? 1.0F : 0.0F) - (leftHeld_ ? 1.0F : 0.0F);
  const float dy = (downHeld_ ? 1.0F : 0.0F) - (upHeld_ ? 1.0F : 0.0F);

  posX_ += dx * speed * dt;
  posY_ += dy * speed * dt;

  const float halfW = (sprite_ != nullptr) ? (spriteW_ * 0.5F) : 32.0F;
  const float halfH = (sprite_ != nullptr) ? (spriteH_ * 0.5F) : 32.0F;
  posX_ = clampf(posX_, halfW, std::max(halfW, static_cast<float>(winW) - halfW));
  posY_ = clampf(posY_, halfH, std::max(halfH, static_cast<float>(winH) - halfH));
}

void App::render() {
  if (!renderer_) {
    return;
  }

  SDL_SetRenderDrawColor(renderer_, 20, 20, 24, 255);
  SDL_RenderClear(renderer_);

  if (sprite_) {
    SDL_FRect dst{
        posX_ - (spriteW_ * 0.5F),
        posY_ - (spriteH_ * 0.5F),
        spriteW_,
        spriteH_,
    };
    SDL_RenderTexture(renderer_, sprite_, nullptr, &dst);
  } else {
    SDL_FRect rect{posX_ - 32.0F, posY_ - 32.0F, 64.0F, 64.0F};
    SDL_SetRenderDrawColor(renderer_, 90, 180, 240, 255);
    SDL_RenderFillRect(renderer_, &rect);
  }

  SDL_RenderPresent(renderer_);
}
