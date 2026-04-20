#pragma once

#include "common/aliases.h"

// Piece enum
// Do NOT change the order of this enum
enum Piece : i8 {
  PAWN,
  ROOK,
  KNIGHT,
  BISHOP,
  QUEEN,
  KING,
  _PIECE_COUNT,
};

// Color enum
enum Color : bool {
  WHITE = false,
  BLACK = true,
};

const i8 PIECE_COUNT = i8(_PIECE_COUNT);

const Piece PROMO_PIECES[] = {ROOK, KNIGHT, BISHOP, QUEEN};
