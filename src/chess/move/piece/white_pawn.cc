#include "chess/move/piece/white_pawn.h"

#include <cstdio>

#include "chess/board/mask_operations.h"

Mask WHITE_PAWN_ATK_MASKS[64];

Mask _white_pawn_atk_mask(Pos pos) {
  i8 row = pos / 8;
  i8 col = pos % 8;
  Mask mask = 0ull;
  if (col < 7) {
    mask |= row_col_mask(row + 1, col + 1);
  }
  if (col > 0) {
    mask |= row_col_mask(row + 1, col - 1);
  }
  return mask;
}

__attribute__((constructor(101)))

void init_white_pawn_attack_masks() {
  for (Pos pos = 0; pos < 64; pos++) {
    Mask move = _white_pawn_atk_mask(pos);

    // Update the table
    WHITE_PAWN_ATK_MASKS[pos] = move;
  }
}

Mask white_pawn_atk_mask(Pos pos) { return WHITE_PAWN_ATK_MASKS[pos]; }
