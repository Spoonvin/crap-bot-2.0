#include "chess/board/mask_operations.h"

#include <cmath>
#include <cstdio>

#define LEFT_SLANT 0x0102040810204080ull
#define RIGHT_SLANT 0x8040201008040201ull

Mask BETWEEN_MASKS[64][64];

Mask _between_mask(Pos p1, Pos p2) {
  using std::max, std::min;

  i8 row1 = p1 / 8;
  i8 col1 = p1 % 8;
  i8 row2 = p2 / 8;
  i8 col2 = p2 % 8;

  Mask mask = 0ull;

  i8 row_max = max(row1, row2);
  i8 row_min = min(row1, row2);
  i8 col_max = max(col1, col2);
  i8 col_min = min(col1, col2);

  // Same column
  if (col1 == col2) {
    mask = col_mask(col1);
    mask &= ~bot_mask(row_min + 1);
    mask &= ~top_mask(8 - row_max);
  }

  // Same row
  else if (row1 == row2) {
    mask = row_mask(row1);
    mask &= ~left_mask(col_min + 1);
    mask &= ~right_mask(8 - col_max);
  }

  // Same diagonal
  else if (row_max - row_min == col_max - col_min) {
    // Slanting right
    if ((col1 - col2) / (row1 - row2) > 0) {
      mask = right_slant_mask(p1);
    }

    // Slanting left
    else {
      mask = left_slant_mask(p2);
    }

    mask &= ~bot_mask(row_min + 1);
    mask &= ~top_mask(8 - row_max);
    mask &= ~left_mask(col_min + 1);
    mask &= ~right_mask(8 - col_max);
  }

  return mask;
}

__attribute__((constructor))

void
init_between_masks() {
  for (Pos p1 = 0; p1 < 64; p1++) {
    for (Pos p2 = 0; p2 < 64; p2++) {
      Mask mask;

      if (p1 == p2) {
        mask = 0ull;
      } else {
        mask = _between_mask(p1, p2);
      }

      BETWEEN_MASKS[p1][p2] = mask;
    }
  }
}

// Clears the least significant bit and returns its index
Pos pop_pos(Mask &mask) {
  Pos tr = __builtin_ctzll(mask);
  mask ^= 1ull << tr;
  return tr;
}

Mask pos_mask(Pos pos) { return 1ull << pos; }

// Computes mask between two positions (exclusive ends)
Mask between_mask(Pos p1, Pos p2) { return BETWEEN_MASKS[p1][p2]; }

// Computes mask with all bits set in row
Mask row_mask(i8 row) { return ROW_0 << (row * 8); }

// Computes mask with all bits set in column
Mask col_mask(i8 col) { return COL_0 * (1ull << col); }

// Compute mask at row/col
Mask row_col_mask(i8 row, i8 col) { return 1ull << (row * 8 + col); }

// Masks n rows below
Mask bot_mask(i8 n) {
  Mask mask = 0ull;

  for (i8 i = 0; i < n; i++) {
    mask |= row_mask(i);
  }

  return mask;
}

// Masks n rows above
Mask top_mask(i8 n) {
  Mask mask = 0ull;

  for (i8 i = 0; i < n; i++) {
    mask |= row_mask(7 - i);
  }

  return mask;
}

// Masks n columns to the left
Mask left_mask(i8 n) {
  Mask mask = 0ull;

  for (i8 i = 0; i < n; i++) {
    mask |= col_mask(i);
  }

  return mask;
}

// Masks n columns to the right
Mask right_mask(i8 n) {
  Mask mask = 0ull;

  for (i8 i = 0; i < n; i++) {
    mask |= col_mask(7 - i);
  }

  return mask;
}

// Computes the mask with a right-slanted diagonal through pos
Mask right_slant_mask(Pos pos) {
  const Mask center_mask = RIGHT_SLANT;

  // Compute relative shift
  i8 shift = (pos / 8 - pos % 8);

  // Compute final mask
  Mask mask =
    (shift < 0) ? (center_mask >> -shift * 8) : (center_mask << shift * 8);

  return mask;
}

// Computes the mask with a left-slanted diagonal through pos
Mask left_slant_mask(Pos pos) {
  const Mask center_mask = LEFT_SLANT;

  // Compute relative shift
  i8 shift = (pos / 8 + pos % 8 - 7);

  // Compute final mask
  Mask mask =
    (shift < 0) ? (center_mask >> -shift * 8) : (center_mask << shift * 8);

  return mask;
}

// Shifts mask up by n rows
Mask shift_mask_up(Mask mask, i8 n) { return mask << n * 8; }

// Shifts mask down by n rows
Mask shift_mask_down(Mask mask, i8 n) { return mask >> n * 8; }
