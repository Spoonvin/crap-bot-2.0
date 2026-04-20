#include "model/human.h"

#include "gui/gui.h"
#include "chess/move/movegen.h"

u8 filter_selected(
  MoveList& moves,
  u8 move_count,
  Pos select_pos,
  MoveList& filtered) {
  u8 filtered_count = 0;

  for (u8 i = 0; i < move_count; i++) {
    Move move = moves[i];

    if (move.from() == select_pos) {
      filtered[filtered_count++] = move;
    }
  }

  return filtered_count;
}

Move Human::select_best(Game& game) {
  Pos select_pos = -1;

  MoveList moves;
  GenResult gen_result = gen_legal(game, moves);

  MoveList filtered;
  u8 filtered_count = 0;

  // Wait for player select valid move in GUI
  while (true) {
    // Render the game state (pieces, squares, etc.)
    gui_ctx.render_game(game, select_pos);

    if (select_pos != -1) {
      // Render available moves from selected square
      gui_ctx.render_moves(filtered, filtered_count);

      // Present backbuffer
      gui_ctx.present();

      // Wait for player to click a square
      auto opt = gui_ctx.await_click();

      if (!opt.has_value()) {
        // Player quit
        return Move::null();
      }

      Pos click = opt.value();

      if (select_pos == click) {
        // Player clicked the same square, deselect
        select_pos = -1;
        continue;
      }

      // Find move of clicked square
      for (u8 i = 0; i < filtered_count; i++) {
        Move move = filtered[i];

        if (move.to() != click) {
          continue;
        }

        // Immedietly return non-promo moves
        if (move.type() != PROMOTION) {
          return move;
        }

        // Present promotion options
        gui_ctx.render_promo(game.turn);
        gui_ctx.present();

        // Await promotion selection
        auto opt = gui_ctx.await_promo();

        if (!opt.has_value()) {
          // Player quit
          return Move::null();
        }

        Piece promo_piece = opt.value();

        return Move::promo(move.from(), move.to(), promo_piece);
      }

      // Clicked square was not a valid move, make new selection

      filtered_count =
        filter_selected(moves, gen_result.count, click, filtered);

      select_pos = click;
    } else {
      // Present backbuffer
      gui_ctx.present();

      // Wait for player to click a square
      auto opt = gui_ctx.await_click();

      if (!opt.has_value()) {
        // Player quit
        return Move::null();
      }

      Pos click = opt.value();

      // Filter for moves from clicked square
      filtered_count =
        filter_selected(moves, gen_result.count, click, filtered);

      select_pos = click;
    }
  }
}