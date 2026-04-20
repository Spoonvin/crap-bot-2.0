#pragma once

#include <cstddef>
#include <cstdio>

#include "chess/board/mask_operations.h"
#include "common/types.h"

Mask capped_mask(Pos pos, Mask mmask);

template <size_t MAX_KEY>
void gen_magic_moves(
  Mask capped_moves[64],
  const u64 magic_numbers[64],
  const u8 magic_shifts[64],
  Mask (*const move_mask)(Mask, Pos),
  Mask buffer[64][MAX_KEY]) {
  for (Pos pos = 0; pos < 64; pos++) {
    Mask mask = capped_moves[pos];

    // Store positions of all set bits
    Pos occupiable[12];
    u8 occ_count = 0;
    while (mask) {
      Pos pos = pop_pos(mask);
      occupiable[occ_count++] = pos;
    }

    // Compute total number of occupancy masks
    u64 mask_count = 1ull << occ_count;

    // Iterate over all mask indices, and map from the index to its
    // corresponding occupancy permutation
    for (u64 i = 0; i < mask_count; i++) {
      Mask occupancy = 0ull;

      // Map each bit of the mask index to a bit in the occupancy mask
      for (u8 j = 0; j < occ_count; j++) {
        Mask bit = (i >> j) & 1;
        occupancy |= bit << occupiable[j];
      }

      // Compute the magic key
      u64 magic_num = magic_numbers[pos];
      u8 magic_shift = magic_shifts[pos];
      u64 key = (occupancy * magic_num) >> magic_shift;

      buffer[pos][key] = move_mask(occupancy, pos);
    }
  }
}
