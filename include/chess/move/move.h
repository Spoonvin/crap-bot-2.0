#pragma once

#include "chess/board/piece.h"
#include "common/aliases.h"
#include "common/types.h"

enum MoveType : i8 {
  NORMAL = 0b00,
  PROMOTION = 0b01,
  EN_PASSANT = 0b10,
  CASTLING = 0b11,
};

// A move is represented by a 16-bit integer
// 0-5: From pos
// 6-12: To pos
// 13-14: Move type
// 15-16: Promotion piece
struct Move {
  u16 data;

  static Move normal(Pos from, Pos to);
  static Move ep(Pos from, Pos to);
  static Move cstl(Pos from, Pos to);
  static Move promo(Pos from, Pos to, Piece piece);
  static Move null();

  Pos from() const;
  Pos to() const;
  MoveType type() const;
  Piece promo_piece() const;
  bool is_null() const;
  void invert();
  void store_alg(char buffer[6]) const;
};
