#pragma once

#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "chess/board/board.h"
#include "chess/player.h"
#include "common/types.h"
#include "move/move.h"

#define MAX_FEN 128

struct Game {
  // Acting player color
  Color turn;

  // Mailbox representation
  Board board;

  // Player states
  Player players[2];

  // Position of target en passant square
  Pos ep_pos;

  // Game counters
  u8 hm_clock;
  u32 fm_counter;

  u64 hash;

  void make_move(Move move);
  void make_null_move();

 private:

  // Helper functions

  void next_turn();
  void move_piece(Player& player, Piece piece, Pos from, Pos to);
  
  // Move side-effects

  void make_normal(Player& act, Player& wait, Piece piece, Pos from, Pos to);
  void make_promo(
    Player& act,
    Player& wait,
    Pos from,
    Pos to,
    Piece promo_piece);
  void make_ep(Player& act, Player& wait, Pos from, Pos to);
  void make_cstl(Player& act, Pos from, Pos to);

  // Piece side-effects

  void pawn_effect(Pos from, Pos to);
  void king_effect(Player& act);
  void rook_effect(Player& act, Pos from);

 public:

  // Draw conditions

  bool is_draw();
  bool is_3fr();
  bool is_insuff();
  bool is_50mr();

  // King can die (atomic patch)
  bool is_king_dead();

  void invert();

  void update_hash(Square sq, Pos pos);
  void update_hash(u64 key);

  // In game_fen.cc

  // Undefined behavior for malformed fen strings
  static Game from_fen(const char* fen);
  void _from_fen(const char* fen);
  void store_fen(char buffer[MAX_FEN]);

  // In game_intial.cc

  static Game initial();
  void _initial();
};
