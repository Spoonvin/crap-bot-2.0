#include "chess/move/piece/knight.h"
#include "common/types.h"
#include "chess/board/mask_operations.h"

Mask KNIGHT_ATK_MASKS[64];

__attribute__((constructor(101)))

void init_knight_atk_masks() {
  for (Pos pos = 0; pos < 64; pos++) {
    i8 row = pos / 8;
    i8 col = pos % 8;
    Mask mask = 0ull;

    // Two below
    if (row > 1) {
      if (col > 0) {
        mask |= row_col_mask(row - 2, col - 1);
      }
      if (col < 7) {
        mask |= row_col_mask(row - 2, col + 1);
      }
    }

    // One below
    if (row > 0) {
      if (col > 1) {
        mask |= row_col_mask(row - 1, col - 2);
      }
      if (col < 6) {
        mask |= row_col_mask(row - 1, col + 2);
      }
    }

    // Two above
    if (row < 6) {
      if (col > 0) {
        mask |= row_col_mask(row + 2, col - 1);
      }
      if (col < 7) {
        mask |= row_col_mask(row + 2, col + 1);
      }
    }

    // One above
    if (row < 7) {
      if (col > 1) {
        mask |= row_col_mask(row + 1, col - 2);
      }
      if (col < 6) {
        mask |= row_col_mask(row + 1, col + 2);
      }
    }

    KNIGHT_ATK_MASKS[pos] = mask;
  }
}

Mask knight_atk_mask(Pos pos) {
  return KNIGHT_ATK_MASKS[pos];
}
