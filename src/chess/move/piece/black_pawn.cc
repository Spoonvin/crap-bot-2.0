#include "chess/move/piece/black_pawn.h"

#include "chess/board/mask_operations.h"

Mask BLACK_PAWN_ATK_MASKS[64];

Mask _black_pawn_atk_mask(Pos pos) {
  i8 row = pos / 8;
  i8 col = pos % 8;
  Mask mask = 0ull;
  if (col < 7) {
    mask |= row_col_mask(row - 1, col + 1);
  }
  if (col > 0) {
    mask |= row_col_mask(row - 1, col - 1);
  }
  return mask;
}

__attribute__((constructor(101)))

void init_black_pawn_attack_masks() {
  for (Pos pos = 0; pos < 64; pos++) {
    Mask move = _black_pawn_atk_mask(pos);

    // Update the table
    BLACK_PAWN_ATK_MASKS[pos] = move;
  }
}

Mask black_pawn_atk_mask(Pos pos) { return BLACK_PAWN_ATK_MASKS[pos]; }
