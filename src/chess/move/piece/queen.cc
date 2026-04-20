#include "common/types.h"
#include "chess/move/piece/bishop.h"
#include "chess/move/piece/rook.h"

Mask queen_atk_mask(Mask occupancy, Pos pos) {
  Mask rook_mask = rook_atk_mask(occupancy, pos);
  Mask bishop_mask = bishop_atk_mask(occupancy, pos);
  return rook_mask | bishop_mask;
}
