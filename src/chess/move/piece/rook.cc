#include "chess/move/piece/rook.h"

#include <cassert>

#include "chess/board/mask_operations.h"
#include "chess/move/magic/magic.h"

Mask ROOK_FREE_MOVES[64];
Mask ROOK_CAPPED_MOVES[64];
Mask ROOK_MAGIC_MOVES[64][4096];

const u64 ROOK_MAGIC_NUMBERS[64] = {
  0xa8002c000108020ull,  0x6c00049b0002001ull,  0x100200010090040ull,
  0x2480041000800801ull, 0x280028004000800ull,  0x900410008040022ull,
  0x280020001001080ull,  0x2880002041000080ull, 0xa000800080400034ull,
  0x4808020004000ull,    0x2290802004801000ull, 0x411000d00100020ull,
  0x402800800040080ull,  0xb000401004208ull,    0x2409000100040200ull,
  0x1002100004082ull,    0x22878001e24000ull,   0x1090810021004010ull,
  0x801030040200012ull,  0x500808008001000ull,  0xa08018014000880ull,
  0x8000808004000200ull, 0x201008080010200ull,  0x801020000441091ull,
  0x800080204005ull,     0x1040200040100048ull, 0x120200402082ull,
  0xd14880480100080ull,  0x12040280080080ull,   0x100040080020080ull,
  0x9020010080800200ull, 0x813241200148449ull,  0x491604001800080ull,
  0x100401000402001ull,  0x4820010021001040ull, 0x400402202000812ull,
  0x209009005000802ull,  0x810800601800400ull,  0x4301083214000150ull,
  0x204026458e001401ull, 0x40204000808000ull,   0x8001008040010020ull,
  0x8410820820420010ull, 0x1003001000090020ull, 0x804040008008080ull,
  0x12000810020004ull,   0x1000100200040208ull, 0x430000a044020001ull,
  0x280009023410300ull,  0xe0100040002240ull,   0x200100401700ull,
  0x2244100408008080ull, 0x8000400801980ull,    0x2000810040200ull,
  0x8010100228810400ull, 0x2000009044210200ull, 0x4080008040102101ull,
  0x40002080411d01ull,   0x2005524060000901ull, 0x502001008400422ull,
  0x489a000810200402ull, 0x1004400080a13ull,    0x4000011008020084ull,
  0x26002114058042ull,
};

const u8 ROOK_MAGIC_SHIFTS[64] = {
  52, 53, 53, 53, 53, 53, 53, 52, 53, 54, 54, 54, 54, 54, 54, 53,
  53, 54, 54, 54, 54, 54, 54, 53, 53, 54, 54, 54, 54, 54, 54, 53,
  53, 54, 54, 54, 54, 54, 54, 53, 53, 54, 54, 54, 54, 54, 54, 53,
  53, 54, 54, 54, 54, 54, 54, 53, 52, 53, 53, 53, 53, 53, 53, 52,
};

Mask _rook_atk_mask(Mask occupancy, Pos pos) {
  Mask mask = ROOK_FREE_MOVES[pos];

  i8 row = pos / 8;
  i8 col = pos % 8;

  // Below
  for (i8 i = row - 1; i > 0; i--) {
    Mask rcmask = row_col_mask(i, col);
    if (rcmask & occupancy) {
      mask &= ~bot_mask(i);
      break;
    }
  }

  // Above
  for (i8 i = row + 1; i < 7; i++) {
    Mask rcmask = row_col_mask(i, col);
    if (rcmask & occupancy) {
      mask &= ~top_mask(7 - i);
      break;
    }
  }

  // Left
  for (i8 i = col - 1; i > 0; i--) {
    Mask rcmask = row_col_mask(row, i);
    if (rcmask & occupancy) {
      mask &= ~left_mask(i);
      break;
    }
  }

  // Right
  for (i8 i = col + 1; i < 7; i++) {
    Mask rcmask = row_col_mask(row, i);
    if (rcmask & occupancy) {
      mask &= ~right_mask(7 - i);
      break;
    }
  }

  return mask;
}

__attribute__((constructor(101)))

void init_rook_free_moves() {
  for (Pos pos = 0; pos < 64; pos++) {
    i8 row = pos / 8;
    i8 col = pos % 8;

    // Compute row and column masks
    Mask rmask = row_mask(row);
    Mask cmask = col_mask(col);

    // Compute mask for position
    Mask pmask = pos_mask(pos);

    // Compute the movement mask
    Mask move = (rmask | cmask) & ~pmask;

    // Update the table
    ROOK_FREE_MOVES[pos] = move;
  }
}

__attribute__((constructor(102)))

void init_rook_capped_moves() {
  for (Pos pos = 0; pos < 64; pos++) {
    Mask mmask = ROOK_FREE_MOVES[pos];
    ROOK_CAPPED_MOVES[pos] = capped_mask(pos, mmask);
  }
}

__attribute__((constructor(103)))

void init_rook_magic_moves() {
  gen_magic_moves(
    ROOK_CAPPED_MOVES,
    ROOK_MAGIC_NUMBERS,
    ROOK_MAGIC_SHIFTS,
    _rook_atk_mask,
    ROOK_MAGIC_MOVES);
}

Mask rook_atk_mask(Mask occupancy, Pos pos) {
  occupancy &= ROOK_CAPPED_MOVES[pos];
  u64 magic_num = ROOK_MAGIC_NUMBERS[pos];
  u64 magic_shift = ROOK_MAGIC_SHIFTS[pos];
  u64 key = (occupancy * magic_num) >> magic_shift;
  return ROOK_MAGIC_MOVES[pos][key];
}
