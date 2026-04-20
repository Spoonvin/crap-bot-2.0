#pragma once

#include <cstdlib>

#include "chess/board/piece.h"

enum Square : i8 {
  EMPTY_SQUARE = 0b10000,
  WHITE_PAWN = PAWN,
  WHITE_ROOK = ROOK,
  WHITE_KNIGHT = KNIGHT,
  WHITE_BISHOP = BISHOP,
  WHITE_QUEEN = QUEEN,
  WHITE_KING = KING,
  BLACK_PAWN = PAWN | 0b1000,
  BLACK_ROOK = ROOK | 0b1000,
  BLACK_KNIGHT = KNIGHT | 0b1000,
  BLACK_BISHOP = BISHOP | 0b1000,
  BLACK_QUEEN = QUEEN | 0b1000,
  BLACK_KING = KING | 0b1000
};

Piece stp(Square square);
Color stc(Square square);
Square pcts(Piece piece, Color color);
Square chrts(char c);
char stchr(Square square);
