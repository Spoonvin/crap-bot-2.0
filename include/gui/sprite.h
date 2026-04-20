#pragma once

#include <SDL3/SDL_render.h>

#include "chess/board/piece.h"

struct Sprite {
  SDL_Texture* texture;
  SDL_FRect srcrect;
};

struct PieceSpriteDescriptor {
  Piece piece;
  Color color;
  const char* path;
};

struct SpriteRepo {
  void init(SDL_Renderer* renderer);
  void quit();

  Sprite* get_piece_sprite(Piece piece, Color color);
  Sprite* get_circle_sprite();

 private:

  SDL_Renderer* renderer;

  Sprite piece_sprite_map[2][PIECE_COUNT];
  Sprite circle_sprite;

  void load_sprite(const char* path, Sprite* buffer);
  void load_piece_sprite(const char* path, Piece piece, Color color);
};
