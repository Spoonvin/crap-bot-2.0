#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_rect.h>
#include <SDL3_image/SDL_image.h>

#include <optional>

#include "chess/game.h"
#include "chess/move/movegen.h"
#include "common/aliases.h"
#include "gui/sprite.h"

#define COLOR_WHITE 0xf2e1c3
#define COLOR_SELECTED_WHITE 0xf9f07b
#define COLOR_BLACK 0xc3a082
#define COLOR_SELECTED_BLACK 0xe2cf59
#define COLOR_MASK_HIGHLIGHT 0xf14a83
#define COLOR_MASK_ALPHA 127

using std::optional;

struct GuiContext {
  void init(u32 window_size);
  bool is_init();
  void quit();

  // Returns when user has clicked on a square (or interrupted)
  optional<Pos> await_click();

  // Helper function, returns when user has selected a promotion piece (or
  // interrupted)
  optional<Piece> await_promo();

  void render_game(Game& game, Pos select_pos);
  void render_moves(MoveList& moves, u8 move_count);
  void render_promo(Color turn);
  void render_mask(Mask mask);
  void present();

 private:

  void render_sprite(Sprite* sprite, SDL_FRect& dstrect);
  void render_piece(Piece piece, Color color, SDL_FRect& dstrect);
  void render_circle(SDL_FRect& dstrect);

  SDL_FRect pos_rect(Pos pos);

 private:

  bool _is_init;
  i32 window_size;
  SDL_Window* window;
  SDL_Renderer* renderer;
  SpriteRepo sprite_repo;

  // Set render color (hexadecimal)
  void set_render_color(u32 color, u8 alpha);
};

// Maintain a global graphics context
inline GuiContext gui_ctx{};
