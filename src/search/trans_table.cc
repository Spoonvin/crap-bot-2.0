#include "search/trans_table.h"
#include <iostream>

TTEntry::TTEntry(u64 hash, Move move, u8 depth, i32 score, TTType type) {
    this->hash = hash;
    this->best_move = move;
    this->depth = depth;
    this->score = score;
    this->type = type;
    this->valid = true;
}  

TTEntry::TTEntry() : hash(0), best_move(Move::null()), depth(0), score(0), type(EXACT), valid(false) {}

bool TTEntry::is_valid() const {
    return valid;
}

void TransTable::put(TTEntry entry) {
    u32 idx = (entry.hash & (TT_SIZE-1));

    TTEntry cur_entry = table[idx];
    if (cur_entry.hash == entry.hash && cur_entry.depth >= entry.depth)
        return;
    
    table[idx] = entry;
}

TTEntry TransTable::get(u64 hash) {
    u32 idx = (hash & (TT_SIZE-1));
    TTEntry entry = table[idx];

    if (entry.is_valid() && hash == entry.hash) {
        return entry;
    }

    return TTEntry();
}

void TransTable::init() {
    table = new TTEntry[TT_SIZE];
}

f32 TransTable::valid_ratio(){
    u32 num_valid = 0;
    for (u32 i = 0; i < TT_SIZE; i++) {
        if (table[i].is_valid())
            num_valid++;
    }

    return (f32)num_valid / (f32)TT_SIZE;
}