#pragma once

#include "chess/board/bitboard.h"

enum CastlingFlags : i8 {
  KING_MOVED = 0b001,
  LEFT_ROOK_MOVED = 0b010,
  RIGHT_ROOK_MOVED = 0b100,
};

struct Player {
  i8 cstl_flags;
  Bitboard bb;

  static Player initial();

  bool lrook_moved();
  bool rrook_moved();
  bool king_moved();

  void set_lrook_moved();
  void set_rrook_moved();
  void set_king_moved();
};
