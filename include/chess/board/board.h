#pragma once

#include "bitboard.h"
#include "square.h"

using BoardRow = u64;

struct Board {
  BoardRow rows[8];

  Square& operator[](Pos pos);

  static Board initial();

  void invert();
  void store_bitboards(Bitboard& white_buffer, Bitboard& black_buffer);
  u64 hash() const;
};
