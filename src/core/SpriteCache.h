#pragma once

#include <SDL3/SDL_render.h>

#include <string>
#include <unordered_map>

class SpriteCache {
 public:
  void init(SDL_Renderer* renderer);
  void shutdown();

  SDL_Texture* get(const std::string& path);

 private:
  SDL_Texture* loadTexture(const std::string& path);

  SDL_Renderer* renderer_ = nullptr;
  std::unordered_map<std::string, SDL_Texture*> textures_;
};
