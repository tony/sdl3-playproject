#include "shmup/ShmupGame.h"

#include <SDL3/SDL_render.h>

#include <algorithm>

#include "ecs/Components.h"
#include "shmup/components/ShmupComponents.h"
#include "shmup/config/ShipConfig.h"

namespace shmup {

ShmupGame::ShmupGame() = default;

ShmupGame::~ShmupGame() {
  spriteCache_.shutdown();
}

bool ShmupGame::init(SDL_Renderer* renderer, const Config& cfg) {
  renderer_ = renderer;
  shipId_ = cfg.shipId;
  stageId_ = cfg.stageId;
  spawnX_ = cfg.spawnX;
  spawnY_ = cfg.spawnY;
  lives_ = 3;

  spriteCache_.init(renderer);
  controller_.setLives(lives_);

  spawnPlayerShip();

  return true;
}

void ShmupGame::spawnPlayerShip() {
  const ShipConfig* shipCfg = ShipRegistry::get(shipId_);
  if (shipCfg == nullptr) {
    return;
  }

  controller_.spawnPlayer(world_, *shipCfg, spawnX_, spawnY_);
}

void ShmupGame::tick(TimeStep ts, const InputState& input) {
  if (!running_ || paused_) {
    return;
  }

  // Transfer input to player entity
  updateInput(input);

  // Run game systems
  controller_.update(world_, ts);

  // Update background scroll
  backgroundX_ += kBackgroundScrollSpeed * ts.dt;

  // Track score from World (accumulated by systems)
  score_ = static_cast<int>(world_.score);

  // Check for game over (player lost all lives)
  EntityId playerId = controller_.player();
  if (playerId != kInvalidEntity && world_.registry.valid(playerId)) {
    const auto* ship = world_.registry.try_get<ShipState>(playerId);
    if (ship != nullptr) {
      lives_ = ship->lives;
      if (ship->health <= 0 && ship->lives <= 0) {
        running_ = false;
      }
    }
  }
}

void ShmupGame::updateInput(const InputState& input) {
  EntityId playerId = controller_.player();
  if (playerId == kInvalidEntity || !world_.registry.valid(playerId)) {
    return;
  }

  auto* playerInput = world_.registry.try_get<InputState>(playerId);
  if (playerInput != nullptr) {
    *playerInput = input;
  }
}

void ShmupGame::render(SDL_Renderer* renderer, int viewW, int viewH) {
  renderBackground(renderer, viewW, viewH);
  renderEntities(renderer);
  renderEffects(renderer);
  renderHUD(renderer, viewW, viewH);
}

void ShmupGame::renderBackground(SDL_Renderer* renderer, int viewW, int viewH) {
  // Simple starfield background (placeholder)
  SDL_SetRenderDrawColor(renderer, 8, 8, 24, 255);
  SDL_RenderClear(renderer);

  // Draw some stars based on scroll position
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
  const int starSpacingX = 80;
  const int starSpacingY = 60;
  const float parallax = 0.3F;

  int offsetX = static_cast<int>(backgroundX_ * parallax) % starSpacingX;

  for (int y = 20; y < viewH; y += starSpacingY) {
    for (int x = -offsetX; x < viewW; x += starSpacingX) {
      // Pseudo-random star positions based on grid
      int sx = x + ((y * 7) % 23);
      int sy = y + ((x * 13) % 17);
      if (sx >= 0 && sx < viewW && sy >= 0 && sy < viewH) {
        SDL_RenderPoint(renderer, static_cast<float>(sx), static_cast<float>(sy));
      }
    }
  }
}

void ShmupGame::renderEntities(SDL_Renderer* renderer) {
  // Render projectiles (colored rectangles - sprites optional)
  auto projView = world_.registry.view<ShmupProjectileTag, ShmupProjectileState, Transform>();
  for (auto [entity, proj, t] : projView.each()) {
    // Check for sprite
    const auto* sprite = world_.registry.try_get<ShmupSprite>(entity);
    if (sprite != nullptr && !sprite->texturePath.empty()) {
      SDL_Texture* tex = spriteCache_.get(sprite->texturePath);
      if (tex != nullptr) {
        float w = static_cast<float>(sprite->frameW) * sprite->scale;
        float h = static_cast<float>(sprite->frameH) * sprite->scale;
        SDL_FRect srcRect{static_cast<float>(sprite->frame * sprite->frameW), 0.0F,
                          static_cast<float>(sprite->frameW), static_cast<float>(sprite->frameH)};
        SDL_FRect dstRect{t.pos.x - w * 0.5F + sprite->offsetX,
                          t.pos.y - h * 0.5F + sprite->offsetY, w, h};
        SDL_RenderTexture(renderer, tex, &srcRect, &dstRect);
        continue;
      }
    }
    // Fallback: colored rectangle
    SDL_FRect rect;
    rect.w = 6.0F;
    rect.h = 12.0F;
    rect.x = t.pos.x - rect.w * 0.5F;
    rect.y = t.pos.y - rect.h * 0.5F;

    if (proj.fromPlayer) {
      SDL_SetRenderDrawColor(renderer, 100, 200, 255, 255);  // Blue for player
    } else {
      SDL_SetRenderDrawColor(renderer, 255, 100, 100, 255);  // Red for enemy
    }
    SDL_RenderFillRect(renderer, &rect);
  }

  // Render satellites
  auto satView = world_.registry.view<ShmupSatelliteTag, Transform>();
  for (auto [entity, t] : satView.each()) {
    // Check for sprite
    const auto* sprite = world_.registry.try_get<ShmupSprite>(entity);
    if (sprite != nullptr && !sprite->texturePath.empty()) {
      SDL_Texture* tex = spriteCache_.get(sprite->texturePath);
      if (tex != nullptr) {
        float w = static_cast<float>(sprite->frameW) * sprite->scale;
        float h = static_cast<float>(sprite->frameH) * sprite->scale;
        SDL_FRect srcRect{static_cast<float>(sprite->frame * sprite->frameW), 0.0F,
                          static_cast<float>(sprite->frameW), static_cast<float>(sprite->frameH)};
        SDL_FRect dstRect{t.pos.x - w * 0.5F + sprite->offsetX,
                          t.pos.y - h * 0.5F + sprite->offsetY, w, h};
        SDL_RenderTexture(renderer, tex, &srcRect, &dstRect);
        continue;
      }
    }
    // Fallback: cyan rectangle
    SDL_FRect rect;
    rect.w = 12.0F;
    rect.h = 12.0F;
    rect.x = t.pos.x - rect.w * 0.5F;
    rect.y = t.pos.y - rect.h * 0.5F;
    SDL_SetRenderDrawColor(renderer, 0, 255, 200, 255);
    SDL_RenderFillRect(renderer, &rect);
  }

  // Render player
  auto playerView = world_.registry.view<ShmupPlayerTag, ShipState, Transform>();
  for (auto [entity, ship, t] : playerView.each()) {
    bool rendered = false;

    // Check for sprite
    const auto* sprite = world_.registry.try_get<ShmupSprite>(entity);
    if (sprite != nullptr && !sprite->texturePath.empty()) {
      SDL_Texture* tex = spriteCache_.get(sprite->texturePath);
      if (tex != nullptr) {
        // Skip rendering on invincibility flash frames
        if (ship.invincibleFrames > 0 && (ship.invincibleFrames / 4) % 2 == 0) {
          // Flash: skip every other 4-frame period
        } else {
          float w = static_cast<float>(sprite->frameW) * sprite->scale;
          float h = static_cast<float>(sprite->frameH) * sprite->scale;
          SDL_FRect srcRect{static_cast<float>(sprite->frame * sprite->frameW), 0.0F,
                            static_cast<float>(sprite->frameW), static_cast<float>(sprite->frameH)};
          SDL_FRect dstRect{t.pos.x - w * 0.5F + sprite->offsetX,
                            t.pos.y - h * 0.5F + sprite->offsetY, w, h};
          SDL_RenderTexture(renderer, tex, &srcRect, &dstRect);
        }
        rendered = true;
      }
    }

    // Fallback: green rectangle
    if (!rendered) {
      SDL_FRect rect;
      rect.w = 32.0F;
      rect.h = 24.0F;
      rect.x = t.pos.x - rect.w * 0.5F;
      rect.y = t.pos.y - rect.h * 0.5F;

      if (ship.invincibleFrames > 0 && (ship.invincibleFrames / 4) % 2 == 0) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 128);
      } else if (ship.focused) {
        SDL_SetRenderDrawColor(renderer, 100, 255, 100, 255);
      } else {
        SDL_SetRenderDrawColor(renderer, 50, 200, 50, 255);
      }
      SDL_RenderFillRect(renderer, &rect);
    }

    // Draw hitbox indicator when focused
    if (ship.focused) {
      SDL_FRect hitbox;
      hitbox.w = 6.0F;
      hitbox.h = 6.0F;
      hitbox.x = t.pos.x - hitbox.w * 0.5F;
      hitbox.y = t.pos.y - hitbox.h * 0.5F;
      SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
      SDL_RenderFillRect(renderer, &hitbox);
    }
  }

  // Render enemies
  auto enemyView = world_.registry.view<ShmupEnemyTag, ShmupEnemyState, Transform>();
  for (auto [entity, enemy, t] : enemyView.each()) {
    // Check for sprite
    const auto* sprite = world_.registry.try_get<ShmupSprite>(entity);
    if (sprite != nullptr && !sprite->texturePath.empty()) {
      SDL_Texture* tex = spriteCache_.get(sprite->texturePath);
      if (tex != nullptr) {
        float w = static_cast<float>(sprite->frameW) * sprite->scale;
        float h = static_cast<float>(sprite->frameH) * sprite->scale;
        SDL_FRect srcRect{static_cast<float>(sprite->frame * sprite->frameW), 0.0F,
                          static_cast<float>(sprite->frameW), static_cast<float>(sprite->frameH)};
        SDL_FRect dstRect{t.pos.x - w * 0.5F + sprite->offsetX,
                          t.pos.y - h * 0.5F + sprite->offsetY, w, h};
        SDL_RenderTexture(renderer, tex, &srcRect, &dstRect);
        continue;
      }
    }
    // Fallback: red rectangle
    SDL_FRect rect;
    rect.w = 28.0F;
    rect.h = 20.0F;
    rect.x = t.pos.x - rect.w * 0.5F;
    rect.y = t.pos.y - rect.h * 0.5F;

    if (enemy.invulnerable) {
      SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255);
    } else {
      SDL_SetRenderDrawColor(renderer, 200, 50, 50, 255);
    }
    SDL_RenderFillRect(renderer, &rect);
  }
}

void ShmupGame::renderEffects(SDL_Renderer* renderer) {
  auto view = world_.registry.view<ShmupEffectTag, EffectState, Transform>();

  for (auto [entity, effect, t] : view.each()) {
    float progress =
        static_cast<float>(effect.ageFrames) / static_cast<float>(effect.lifetimeFrames);

    // Clamp progress to [0, 1] for safety
    progress = std::min(progress, 1.0F);

    // Fade out alpha over lifetime
    Uint8 alpha = static_cast<Uint8>((1.0F - progress) * 255.0F);

    // Size expands then contracts
    float sizeMult = effect.scale * (1.0F + progress * 0.5F);

    if (effect.type == EffectType::Explosion) {
      // Orange/yellow expanding circle (rendered as rect for simplicity)
      float radius = 16.0F * sizeMult;
      SDL_FRect rect;
      rect.w = radius * 2.0F;
      rect.h = radius * 2.0F;
      rect.x = t.pos.x - radius;
      rect.y = t.pos.y - radius;

      // Color shifts from yellow to orange to red
      Uint8 green = static_cast<Uint8>(200.0F * (1.0F - progress));
      SDL_SetRenderDrawColor(renderer, 255, green, 50, alpha);
      SDL_RenderFillRect(renderer, &rect);
    } else if (effect.type == EffectType::HitSpark) {
      // Small white flash that shrinks
      float size = 8.0F * sizeMult * (1.0F - progress);
      SDL_FRect rect;
      rect.w = size;
      rect.h = size;
      rect.x = t.pos.x - size * 0.5F;
      rect.y = t.pos.y - size * 0.5F;

      SDL_SetRenderDrawColor(renderer, 255, 255, 200, alpha);
      SDL_RenderFillRect(renderer, &rect);
    }
  }
}

void ShmupGame::renderHUD(SDL_Renderer* renderer, int viewW, int viewH) {
  (void)viewH;

  // Draw score and lives at top
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

  // Score display (placeholder - just render as text using debug renderer)
  char scoreText[64];
  snprintf(scoreText, sizeof(scoreText), "SCORE: %08d", score_);
  SDL_RenderDebugText(renderer, 10.0F, 10.0F, scoreText);

  // Lives display
  char livesText[32];
  snprintf(livesText, sizeof(livesText), "LIVES: %d", lives_);
  SDL_RenderDebugText(renderer, static_cast<float>(viewW) - 100.0F, 10.0F, livesText);

  // Weapon level display
  EntityId playerId = controller_.player();
  if (playerId != kInvalidEntity && world_.registry.valid(playerId)) {
    const auto* weapons = world_.registry.try_get<WeaponState>(playerId);
    if (weapons != nullptr && !weapons->mounts.empty()) {
      int level = weapons->mounts[0].level;
      char levelText[16];
      snprintf(levelText, sizeof(levelText), "LV:%d", level);
      SDL_RenderDebugText(renderer, static_cast<float>(viewW) - 100.0F, 26.0F, levelText);
    }
  }

  // Game over message
  if (!running_) {
    const char* gameOver = "GAME OVER";
    SDL_RenderDebugText(renderer, static_cast<float>(viewW) / 2.0F - 40.0F,
                        static_cast<float>(viewH) / 2.0F, gameOver);
  }
}

void ShmupGame::reset() {
  // Clear all entities
  world_.registry.clear();
  world_.player = kInvalidEntity;
  world_.enemyKills = 0;
  world_.score = 0;

  // Reset state
  running_ = true;
  paused_ = false;
  score_ = 0;
  lives_ = 3;
  backgroundX_ = 0.0F;

  controller_.setLives(lives_);

  // Respawn player
  spawnPlayerShip();
}

}  // namespace shmup
