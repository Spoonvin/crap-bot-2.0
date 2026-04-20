#pragma once

#include "common/aliases.h"
#include "chess/move/move.h"

enum TTType {
    EXACT,
    UPPER,
    LOWER
};

struct TTEntry {
    u64 hash;
    Move best_move;
    u8 depth;
    i32 score;
    TTType type;
};

struct TransTable {

};