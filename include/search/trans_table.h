#pragma once

#include "common/aliases.h"
#include "chess/move/move.h"
#include "search/evaluation.h"

#define TT_SIZE 33554432
#define UNKNOWN_TT_VALUE (MIN_VALUE-1)

enum TTType : u8 {
    EXACT,
    UPPER,
    LOWER
};

struct TTEntry {
    //u64 hash;

    //        Bits
    // Move:  0  - 15
    // depth: 16 - 23
    // score: 24 - 55
    // type:  56 - 57
    u64 data;
    u64 key;
    u16 age;

    /*
    Move best_move;
    u8 depth;
    i32 score;
    TTType type;*/

    TTEntry(u64 hash, Move move, u8 depth, i32 score, TTType type);
    TTEntry();

    bool is_valid();

    Move get_move() const;
    u8 get_depth() const;
    i32 get_score() const;
    TTType get_type() const;
};

struct TransTable {
    TTEntry* table;
    u16 age;

    ~TransTable();

    void put(TTEntry entry, u64 hash);

    TTEntry get(u64 hash);
    Move get_pv_move(u64 hash);


    void init();
    f32 valid_ratio();
};