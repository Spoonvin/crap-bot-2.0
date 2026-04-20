#pragma once

#include "common/types.h"

#define COL_0 0x0101010101010101ull
#define COL_1 0x0202020202020202ull
#define COL_2 0x0404040404040404ull
#define COL_3 0x0808080808080808ull
#define COL_4 0x1010101010101010ull
#define COL_5 0x2020202020202020ull
#define COL_6 0x4040404040404040ull
#define COL_7 0x8080808080808080ull

#define ROW_0 0xffull
#define ROW_1 0xff00ull
#define ROW_2 0xff0000ull
#define ROW_3 0xff000000ull
#define ROW_4 0xff00000000ull
#define ROW_5 0xff0000000000ull
#define ROW_6 0xff000000000000ull
#define ROW_7 0xff00000000000000ull

#define FULL_BOARD 0xffffffffffffffffull

#define WHITE_CSTL_Q 0xeull
#define WHITE_CSTL_K 0x60ull

#define BLACK_CSTL_Q 0xe00000000000000ull
#define BLACK_CSTL_K 0x6000000000000000ull

#define LEFT_SLANT 0x0102040810204080ull
#define RIGHT_SLANT 0x8040201008040201ull

#define BLACK_SQUARES 0xaa55aa55aa55aa55ull
#define WHITE_SQUARES 0x55aa55aa55aa55aaull

extern Mask BETWEEN_MASKS[64][64];

Pos pop_pos(Mask &mask);

Mask pos_mask(Pos pos);
Mask between_mask(Pos p1, Pos p2);
Mask row_mask(i8 row);
Mask col_mask(i8 col);
Mask row_col_mask(i8 row, i8 col);
Mask bot_mask(i8 n);
Mask top_mask(i8 n);
Mask left_mask(i8 n);
Mask right_mask(i8 n);
Mask right_slant_mask(Pos pos);
Mask left_slant_mask(Pos pos);
Mask shift_mask_up(Mask mask, i8 n);
Mask shift_mask_down(Mask mask, i8 n);
