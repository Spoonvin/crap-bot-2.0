#include "chess/move/piece/bishop.h"

#include "common/aliases.h"
#include "chess/board/mask_operations.h"
#include "chess/move/magic/magic.h"

Mask BISHOP_FREE_MOVES[64];
Mask BISHOP_CAPPED_MOVES[64];
Mask BISHOP_MAGIC_MOVES[64][512];

const u64 BISHOP_MAGIC_NUMBERS[64] = {
  0x89a1121896040240ull, 0x2004844802002010ull, 0x2068080051921000ull,
  0x62880a0220200808ull, 0x4042004000000ull,    0x100822020200011ull,
  0xc00444222012000aull, 0x28808801216001ull,   0x400492088408100ull,
  0x201c401040c0084ull,  0x840800910a0010ull,   0x82080240060ull,
  0x2000840504006000ull, 0x30010c4108405004ull, 0x1008005410080802ull,
  0x8144042209100900ull, 0x208081020014400ull,  0x4800201208ca00ull,
  0xf18140408012008ull,  0x1004002802102001ull, 0x841000820080811ull,
  0x40200200a42008ull,   0x800054042000ull,     0x88010400410c9000ull,
  0x520040470104290ull,  0x1004040051500081ull, 0x2002081833080021ull,
  0x400c00c010142ull,    0x941408200c002000ull, 0x658810000806011ull,
  0x188071040440a00ull,  0x4800404002011c00ull, 0x104442040404200ull,
  0x511080202091021ull,  0x4022401120400ull,    0x80c0040400080120ull,
  0x8040010040820802ull, 0x480810700020090ull,  0x102008e00040242ull,
  0x809005202050100ull,  0x8002024220104080ull, 0x431008804142000ull,
  0x19001802081400ull,   0x200014208040080ull,  0x3308082008200100ull,
  0x41010500040c020ull,  0x4012020c04210308ull, 0x208220a202004080ull,
  0x111040120082000ull,  0x6803040141280a00ull, 0x2101004202410000ull,
  0x8200000041108022ull, 0x21082088000ull,      0x2410204010040ull,
  0x40100400809000ull,   0x822088220820214ull,  0x40808090012004ull,
  0x910224040218c9ull,   0x402814422015008ull,  0x90014004842410ull,
  0x1000042304105ull,    0x10008830412a00ull,   0x2520081090008908ull,
  0x40102000a0a60140ull,
};

const u8 BISHOP_MAGIC_SHIFTS[64] = {
  58, 59, 59, 59, 59, 59, 59, 58, 59, 59, 59, 59, 59, 59, 59, 59,
  59, 59, 57, 57, 57, 57, 59, 59, 59, 59, 57, 55, 55, 57, 59, 59,
  59, 59, 57, 55, 55, 57, 59, 59, 59, 59, 57, 57, 57, 57, 59, 59,
  59, 59, 59, 59, 59, 59, 59, 59, 58, 59, 59, 59, 59, 59, 59, 58,
};

Mask _bishop_atk_mask(Mask occupancy, Pos pos) {
  Mask mask = BISHOP_FREE_MOVES[pos];

  i8 row = pos / 8;
  i8 col = pos % 8;

  // Bottom-left diagonal
  for (i8 i = row - 1, j = col - 1; i > 0 && j > 0; --i, --j) {
    Mask rcmask = row_col_mask(i, j);
    if (occupancy & rcmask) {
      mask &= ~(bot_mask(i) & left_mask(j));
      break;
    }
  }

  // Bottom-right diagonal
  for (i8 i = row - 1, j = col + 1; i > 0 && j < 7; --i, ++j) {
    Mask rcmask = row_col_mask(i, j);
    if (occupancy & rcmask) {
      mask &= ~(bot_mask(i) & right_mask(7 - j));
      break;
    }
  }

  // Top-left diagonal
  for (i8 i = row + 1, j = col - 1; i < 7 && j > 0; ++i, --j) {
    Mask rcmask = row_col_mask(i, j);
    if (occupancy & rcmask) {
      mask &= ~(top_mask(7 - i) & left_mask(j));
      break;
    }
  }

  // Top-right diagonal
  for (i8 i = row + 1, j = col + 1; i < 7 && j < 7; ++i, ++j) {
    Mask rcmask = row_col_mask(i, j);
    if (occupancy & rcmask) {
      mask &= ~(top_mask(7 - i) & right_mask(7 - j));
      break;
    }
  }

  return mask;
}

__attribute__((constructor(101))) 

void init_bishop_free_moves() {
  for (Pos pos = 0; pos < 64; pos++) {
    Mask lsmask = right_slant_mask(pos);
    Mask rsmask = left_slant_mask(pos);
    Mask pmask = pos_mask(pos);
    Mask move = (lsmask | rsmask) & ~pmask;
    BISHOP_FREE_MOVES[pos] = move;
  }
}

__attribute__((constructor(102))) 

void init_bishop_capped_moves() {
  for (Pos pos = 0; pos < 64; pos++) {
    BISHOP_CAPPED_MOVES[pos] = capped_mask(pos, BISHOP_FREE_MOVES[pos]);
  }
}

__attribute__((constructor(103)))

void init_bishop_magic_moves() {
  gen_magic_moves(
    BISHOP_CAPPED_MOVES,
    BISHOP_MAGIC_NUMBERS,
    BISHOP_MAGIC_SHIFTS,
    _bishop_atk_mask,
    BISHOP_MAGIC_MOVES);
}

Mask bishop_atk_mask(Mask occupancy, Pos pos) {
  occupancy &= BISHOP_CAPPED_MOVES[pos];
  u64 magic_num = BISHOP_MAGIC_NUMBERS[pos];
  u64 magic_shift = BISHOP_MAGIC_SHIFTS[pos];
  u64 key = (occupancy * magic_num) >> magic_shift;
  return BISHOP_MAGIC_MOVES[pos][key];
}
