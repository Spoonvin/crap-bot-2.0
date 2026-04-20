#include "chess/board/bitboard.h"

Mask& Bitboard::operator[](Piece piece) { return masks[piece]; }

Mask Bitboard::occupancy() {
  Mask mask = 0ull;

  for (i8 piece = 0; piece < PIECE_COUNT; piece++) {
    mask |= masks[piece];
  }

  return mask;
}
