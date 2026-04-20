#include "chess/move/magic/magic.h"

#include "chess/board/mask_operations.h"

Mask capped_mask(Pos pos, Mask mask) {
  i8 row = pos / 8;
  i8 col = pos % 8;

  if (row != 0) {
    mask &= ~row_mask(0);
  }

  if (row != 7) {
    mask &= ~row_mask(7);
  }

  if (col != 0) {
    mask &= ~col_mask(0);
  }

  if (col != 7) {
    mask &= ~col_mask(7);
  }

  return mask;
}
