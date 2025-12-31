#include "core/SpriteCache.h"

#include <cstdio>
#include <utility>

#include <SDL3/SDL_blendmode.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_surface.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"  // NOLINT(build/include_subdir)

namespace {

bool endsWith(const std::string& str, const std::string& suffix) {
  if (suffix.size() > str.size())
    return false;
  return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool isPngFile(const std::string& path) {
  return endsWith(path, ".png") || endsWith(path, ".PNG");
}

SDL_Surface* loadPngWithStb(const char* path) {
  int width = 0;
  int height = 0;
  int channels = 0;

  // Request RGBA (4 channels)
  unsigned char* data = stbi_load(path, &width, &height, &channels, 4);
  if (!data) {
    std::printf("stbi_load failed: %s (%s)\n", path, stbi_failure_reason());
    return nullptr;
  }

  // Create SDL surface from the pixel data
  // SDL3 uses SDL_PIXELFORMAT_RGBA32 which matches stb_image's RGBA output
  SDL_Surface* surface =
      SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32, data, width * 4);

  if (!surface) {
    std::printf("SDL_CreateSurfaceFrom failed: %s (%s)\n", path, SDL_GetError());
    stbi_image_free(data);
    return nullptr;
  }

  // SDL_CreateSurfaceFrom doesn't copy data, so we need to copy it
  SDL_Surface* copied = SDL_DuplicateSurface(surface);
  SDL_DestroySurface(surface);
  stbi_image_free(data);

  return copied;
}

}  // namespace

void SpriteCache::init(SDL_Renderer* renderer) {
  renderer_ = renderer;
}

void SpriteCache::shutdown() {
  for (auto& [path, tex] : textures_) {
    (void)path;
    if (tex)
      SDL_DestroyTexture(tex);
  }
  textures_.clear();
  renderer_ = nullptr;
}

SDL_Texture* SpriteCache::get(const std::string& path) {
  auto it = textures_.find(path);
  if (it != textures_.end())
    return it->second;

  SDL_Texture* tex = loadTexture(path);
  if (!tex)
    return nullptr;

  textures_.emplace(path, tex);
  return tex;
}

SDL_Texture* SpriteCache::loadTexture(const std::string& path) {
  if (!renderer_)
    return nullptr;

  SDL_Surface* surface = nullptr;

  if (isPngFile(path)) {
    // PNG: Load via stb_image, uses alpha channel for transparency
    surface = loadPngWithStb(path.c_str());
    if (!surface)
      return nullptr;
  } else {
    // BMP: Load via SDL, uses magenta color key for transparency
    surface = SDL_LoadBMP(path.c_str());
    if (!surface) {
      std::printf("SDL_LoadBMP failed: %s (%s)\n", path.c_str(), SDL_GetError());
      return nullptr;
    }
    Uint32 key = SDL_MapSurfaceRGB(surface, 255, 0, 255);
    (void)SDL_SetSurfaceColorKey(surface, true, key);
  }

  SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surface);
  SDL_DestroySurface(surface);

  if (!tex) {
    std::printf("SDL_CreateTextureFromSurface failed: %s (%s)\n", path.c_str(), SDL_GetError());
    return nullptr;
  }

  (void)SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
  (void)SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

  return tex;
}
