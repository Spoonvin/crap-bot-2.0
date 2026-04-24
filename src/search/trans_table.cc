#include "search/trans_table.h"
#include <iostream>

#define INVALID_KEY 0

TTEntry::TTEntry(u64 hash, Move move, u8 depth, i32 score, TTType type) {
    /*
    this->hash = hash;
    this->best_move = move;
    this->depth = depth;
    this->score = score;
    this->type = type;*/

    this->data =
        ((u64)move.data & 0xFFFFULL) |
        (((u64)depth & 0xFFULL) << 16) |
        (((u64)score & 0xFFFFFFFFULL) << 24) |
        (((u64)type & 0xFFULL) << 56);
    this->key = hash ^ data;
    this->age = 0;
}  

TTEntry::TTEntry() : data(0), key(0) , age(0) {}

Move TTEntry::get_move() const {
    Move move;
    move.data = (u16)(this->data);
    return move;
}

u8 TTEntry::get_depth() const {
    return (u8)(this->data >> 16);
}

i32 TTEntry::get_score() const {
    return (i32)(this->data >> 24);
}

TTType TTEntry::get_type() const {
    return (TTType)((this->data >> 56));
}

TransTable::~TransTable() {
    delete[] table;
}

void TransTable::put(TTEntry entry, u64 hash) {
    u32 idx = (hash & (TT_SIZE-1));

    TTEntry cur_entry = table[idx];
    
    bool replace = false;

    if (!cur_entry.is_valid()) {
        replace = true;
    } else {
        if (cur_entry.age < this->age) {
            replace = true;
        } else if (cur_entry.get_depth() < entry.get_depth()) {
            replace = true;
        }
    }

    if (!replace) return;

    entry.age = this->age;
    table[idx] = entry;
}

TTEntry TransTable::get(u64 hash) {
    u32 idx = (hash & (TT_SIZE-1));
    TTEntry entry = table[idx];

    u64 key = hash ^ entry.data;

    if (entry.is_valid() && key == entry.key) {
        return entry;
    }

    return TTEntry();
}

Move TransTable::get_pv_move(u64 hash) {
    return this->get(hash).get_move();
}

void TransTable::init() {
    table = new TTEntry[TT_SIZE];
    age = 0;
}

bool TTEntry::is_valid() {
    return key != INVALID_KEY;
}

f32 TransTable::valid_ratio(){
    u32 num_valid = 0;
    for (u32 i = 0; i < TT_SIZE; i++) {
        if (table[i].is_valid())
            num_valid++;
    }

    return (f32)num_valid / (f32)TT_SIZE;
}

