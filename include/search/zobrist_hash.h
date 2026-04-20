#pragma once

#include "common/aliases.h"
#include "chess/board/piece.h"
#include <random>
#include <limits>

#define EP_ZOB_INDEX 12

// Random hash keys for every square. One for each piece of each color and en-passant position.
u64 sq_key_map[64][PIECE_COUNT*2 + 1];

u64 acting_player_key; // If white to play
u64 w_cstl_left_key;
u64 w_cstl_right_key;
u64 b_cstl_left_key;
u64 b_cstl_right_key;

u64 gen_zob_hash(Game& game);