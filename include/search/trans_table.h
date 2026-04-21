#pragma once

#include "common/aliases.h"
#include "chess/move/move.h"
#include "search/evaluation.h"

#define TT_SIZE 16777216
#define UNKNOWN_TT_VALUE (MIN_VALUE-1)

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
    bool valid;

    TTEntry(u64 hash, Move move, u8 depth, i32 score, TTType type);
    TTEntry();

    bool is_valid() const;
};

struct TransTable {
    TTEntry* table;

    void put(TTEntry entry);

    TTEntry get(u64 hash);

    void init();
    f32 valid_ratio();
};