#pragma once

#include <atomic>
#include <cstddef>
#include <memory>

#include "common/aliases.h"
#include "chess/move/move.h"
#include "search/evaluation.h"

#define TT_SIZE (16777216*2)
#define UNKNOWN_TT_VALUE (MIN_VALUE-1)

enum TTType : u8 {
    EXACT,
    UPPER,
    LOWER
};

struct TTEntry {
    // Local snapshot; shared storage lives in TTSlot.
    //        Bits
    // Move:  0  - 15
    // depth: 16 - 23
    // score: 24 - 55
    // type:  56 - 57
    u64 data;
    u64 key;
    u16 age;

    TTEntry(u64 hash, Move move, u8 depth, i32 score, TTType type);
    TTEntry();

    bool is_valid() const;

    Move get_move() const;
    u8 get_depth() const;
    i32 get_score() const;
    TTType get_type() const;
};

struct TTSlot {
    std::atomic<u64> data{0};
    std::atomic<u64> key{0};
    std::atomic<u16> age{0};

    // Fields can come from different writes. Validate the returned key/data
    // together before using a snapshot as a search result.
    TTEntry load() const;
    void store(const TTEntry& entry);
};

struct TransTable {
    std::unique_ptr<TTSlot[]> table;
    size_t size;
    // Advance only when no searches are using the table (after worker joins).
    u16 age;

    TransTable();

    void put(TTEntry entry, u64 hash);

    TTEntry get(u64 hash) const;
    Move get_pv_move(u64 hash) const;


    // Clear between searches, with no active workers.
    void init();
    void resize(unsigned int megabytes);
    f32 valid_ratio() const;
};
