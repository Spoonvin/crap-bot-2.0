#pragma once

#include "common/aliases.h"
#include "chess/board/piece.h"
#include "chess/game.h"

#include <random>
#include <limits>

#define EP_ZOB_INDEX 12

// Random hash keys for every square. One for each piece of each color and en-passant position.
extern u64 sq_key_map[64][PIECE_COUNT*2 + 1];

extern u64 acting_player_key; // If white to play
extern u64 w_cstl_left_key;
extern u64 w_cstl_right_key;
extern u64 b_cstl_left_key;
extern u64 b_cstl_right_key;

void init_hash_key_map();

u64 gen_zob_hash(Game& game);