#include "chess/move/piece/king.h"

#include "chess/board/mask_operations.h"

Mask KING_ATK_MASKS[64];

__attribute__((constructor(101)))

void init_king_atk_masks() {
  for (Pos pos = 0; pos < 64; pos++) {
    i8 row = pos / 8;
    i8 col = pos % 8;
    Mask mask = 0ull;

    // Left
    if (col > 0) {
      mask |= row_col_mask(row, col - 1);
    }

    // Right
    if (col < 7) {
      mask |= row_col_mask(row, col + 1);
    }

    // Below
    if (row > 0) {
      // Bottom
      mask |= row_col_mask(row - 1, col);

      // Bottom-left
      if (col > 0) {
        mask |= row_col_mask(row - 1, col - 1);
      }

      // Bottom-right
      if (col < 7) {
        mask |= row_col_mask(row - 1, col + 1);
      }
    }

    // Above
    if (row < 7) {
      // Top
      mask |= row_col_mask(row + 1, col);

      // Top-left
      if (col > 0) {
        mask |= row_col_mask(row + 1, col - 1);
      }

      // Top-right
      if (col < 7) {
        mask |= row_col_mask(row + 1, col + 1);
      }
    }

    KING_ATK_MASKS[pos] = mask;
  }
}

Mask king_atk_mask(Pos pos) { return KING_ATK_MASKS[pos]; }
