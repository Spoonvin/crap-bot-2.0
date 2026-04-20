#include "gui/sprite.h"

#include <SDL3_image/SDL_image.h>

#include <cstdio>
#include <iterator>

void SpriteRepo::init(SDL_Renderer* renderer) {
  this->renderer = renderer;

  // Map texture names to encoded piece indices
  PieceSpriteDescriptor descriptors[] = {
    {PAWN, WHITE, "pawn-w.svg"},
    {PAWN, BLACK, "pawn-b.svg"},
    {KNIGHT, WHITE, "knight-w.svg"},
    {KNIGHT, BLACK, "knight-b.svg"},
    {BISHOP, WHITE, "bishop-w.svg"},
    {BISHOP, BLACK, "bishop-b.svg"},
    {ROOK, WHITE, "rook-w.svg"},
    {ROOK, BLACK, "rook-b.svg"},
    {QUEEN, WHITE, "queen-w.svg"},
    {QUEEN, BLACK, "queen-b.svg"},
    {KING, WHITE, "king-w.svg"},
    {KING, BLACK, "king-b.svg"},
  };

  // Load piece sprites
  for (size_t i = 0; i < std::size(descriptors); i++) {
    auto& descriptor = descriptors[i];
    char path[128];
    sprintf(path, "assets/pieces/%s", descriptor.path);
    load_piece_sprite(path, descriptor.piece, descriptor.color);
  }

  // Load circle sprite
  load_sprite("assets/circle.svg", &this->circle_sprite);
}

void SpriteRepo::quit() {
  // Free piece sprite textures
  for (size_t i = 0; i < 2; i++) {
    for (size_t j = 0; j < PIECE_COUNT; j++) {
      SDL_DestroyTexture(piece_sprite_map[i][j].texture);
    }
  }

  // Free circle sprite texture
  SDL_DestroyTexture(circle_sprite.texture);
}

void SpriteRepo::load_sprite(const char* path, Sprite* buffer) {
  SDL_Surface* surface = IMG_Load(path);
  if (surface == nullptr) {
    abort();
  }

  buffer->texture = SDL_CreateTextureFromSurface(renderer, surface);

  if (buffer->texture == nullptr) {
    abort();
  }

  buffer->srcrect.x = 0.f;
  buffer->srcrect.y = 0.f;

  SDL_GetTextureSize(buffer->texture, &buffer->srcrect.w, &buffer->srcrect.h);

  // Free surface after use
  SDL_DestroySurface(surface);
}

void SpriteRepo::load_piece_sprite(const char* path, Piece piece, Color color) {
  Sprite& sprite = piece_sprite_map[color][piece];
  load_sprite(path, &sprite);
}

Sprite* SpriteRepo::get_piece_sprite(Piece piece, Color color) {
  return &piece_sprite_map[color][piece];
}

Sprite* SpriteRepo::get_circle_sprite() { return &circle_sprite; }
