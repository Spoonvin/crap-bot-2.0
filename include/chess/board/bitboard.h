#pragma once

#include "common/types.h"
#include "chess/board/piece.h"

struct Bitboard {
  Mask masks[PIECE_COUNT];

  Mask& operator[](Piece piece);
  Mask occupancy();
};
