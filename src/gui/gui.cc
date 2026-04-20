#include "gui/gui.h"

#include <SDL3/SDL_blendmode.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_oldnames.h>
#include <SDL3/SDL_render.h>

#include "chess/board/mask_operations.h"

void GuiContext::init(u32 window_size) {
  // Initialize SDL with video capabilities and event handling
  // IMG_Init is no longer required as of sdl3-image version 3.1.1
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    fprintf(stderr, "error: Could not initialize SDL: %s\n", SDL_GetError());
    exit(EXIT_FAILURE);
  }

  // Window size kept for computing screen offsets
  this->window_size = window_size;

  // Create window and renderer
  window = SDL_CreateWindow("Chess Engine", window_size, window_size, 0);
  renderer = SDL_CreateRenderer(window, NULL);
  SDL_SetRenderVSync(renderer, SDL_RENDERER_VSYNC_ADAPTIVE);

  // Error creating window or renderer
  if (!(window && renderer)) {
    const char* sdl_error = SDL_GetError();
    fprintf(stderr, "error: Could not create window or renderer: %s\n", sdl_error);
    exit(EXIT_FAILURE);
  }

  // Initialize sprite repository
  sprite_repo.init(renderer);

  _is_init = true;
}

bool GuiContext::is_init() { return _is_init; }

void GuiContext::quit() {
  // Destroy textures
  sprite_repo.quit();

  // Free SDL
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

void GuiContext::set_render_color(u32 color, u8 alpha) {
  // Separate color channels for rgba
  u8 red_channel = (color >> 16) & 0xff;
  u8 green_channel = (color >> 8) & 0xff;
  u8 blue_channel = color & 0xff;

  SDL_SetRenderDrawColor(
    renderer,
    red_channel,
    green_channel,
    blue_channel,
    alpha);
}

void GuiContext::render_game(Game& game, Pos select_pos) {
  for (Pos pos = 0; pos < 64; pos++) {
    // Compute is-selected
    bool is_selected = select_pos == pos;

    // Compute and set square color
    bool is_white = ((pos >> 3) + (pos & 7)) % 2 == 0;
    u32 color;
    if (is_white) {
      color = is_selected ? COLOR_SELECTED_WHITE : COLOR_WHITE;
    } else {
      color = is_selected ? COLOR_SELECTED_BLACK : COLOR_BLACK;
    }
    set_render_color(color, 255);

    // Render square
    SDL_FRect rect = pos_rect(pos);
    SDL_RenderFillRect(renderer, &rect);

    // Render piece on non-empty square
    // Segfault here if game_instance isn't set
    Square& square = game.board[pos];
    if (!(square & EMPTY_SQUARE)) {
      Piece piece = stp(square);
      Color color = stc(square);
      render_piece(piece, color, rect);
    }
  }
}

// Renders circles over moves
void GuiContext::render_moves(MoveList& moves, u8 moves_size) {
  for (u8 i = 0; i < moves_size; i++) {
    Move move = moves[i];
    Pos to_pos = move.to();
    SDL_FRect rect = pos_rect(to_pos);
    render_circle(rect);
  }
}

void GuiContext::render_promo(Color turn) {
  set_render_color(0xffffff, 255);
  SDL_RenderClear(renderer);

  for (u8 i = 0; i < 4; i++) {
    SDL_FRect rect = pos_rect(i);
    render_piece(PROMO_PIECES[i], turn, rect);
  }
}

void GuiContext::render_mask(Mask mask) {
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_MUL);
  set_render_color(COLOR_MASK_HIGHLIGHT, COLOR_MASK_ALPHA);

  while (mask) {
    Pos pos = pop_pos(mask);
    // Render square
    SDL_FRect rect = pos_rect(pos);
    SDL_RenderFillRect(renderer, &rect);
  }

  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

void GuiContext::present() { SDL_RenderPresent(renderer); }

// Returns nullopt on quit
optional<Pos> GuiContext::await_click() {
  SDL_Event e;
  while (true) {
    SDL_WaitEvent(&e);
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
      f32 x, y;

      // Capture cursor position
      SDL_GetMouseState(&x, &y);

      f32 square_size = window_size / 8.f;

      i8 row = (window_size - y) / square_size;
      i8 col = x / square_size;

      return row * 8 + col;
    } else if (e.type == SDL_EVENT_QUIT) {
      return {};
    }
  }
}

// Returns nullopt on quit
optional<Piece> GuiContext::await_promo() {
  while (true) {
    auto opt = await_click();

    if (!opt.has_value()) {
      return {};
    }

    i8 idx = opt.value();

    // Return piece if valid
    if (idx >= 0 && idx < 4) {
      return PROMO_PIECES[idx];
    }
  }
}

void GuiContext::render_sprite(Sprite* sprite, SDL_FRect& dstrect) {
  SDL_RenderTexture(renderer, sprite->texture, &sprite->srcrect, &dstrect);
}

void GuiContext::render_piece(Piece piece, Color color, SDL_FRect& dstrect) {
  Sprite* sprite = sprite_repo.get_piece_sprite(piece, color);
  render_sprite(sprite, dstrect);
}

void GuiContext::render_circle(SDL_FRect& dstrect) {
  render_sprite(sprite_repo.get_circle_sprite(), dstrect);
}

SDL_FRect GuiContext::pos_rect(Pos pos) {
  i8 row = pos >> 3;
  i8 col = pos & 7;
  f32 square_size = window_size / 8.f;
  f32 square_y = window_size - square_size * (row + 1);
  f32 square_x = square_size * col;
  return {square_x, square_y, square_size, square_size};
}
