#include "search/trans_table.h"

#define INVALID_KEY 0

TTEntry::TTEntry(u64 hash, Move move, u8 depth, i32 score, TTType type) {
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

TTEntry TTSlot::load() const {
    TTEntry entry;
    entry.data = data.load(std::memory_order_relaxed);
    entry.key = key.load(std::memory_order_relaxed);
    entry.age = age.load(std::memory_order_relaxed);
    return entry;
}

void TTSlot::store(const TTEntry& entry) {
    // Relaxed accesses are enough for this probabilistically validated cache:
    // stale/mixed snapshots and lost replacements are allowed, data races aren't.
    data.store(entry.data, std::memory_order_relaxed);
    key.store(entry.key, std::memory_order_relaxed);
    age.store(entry.age, std::memory_order_relaxed);
}

TransTable::TransTable()
    : table(std::make_unique<TTSlot[]>(TT_SIZE)), size(TT_SIZE), age(0) {}

void TransTable::resize(unsigned int megabytes) {
    const size_t capacity = static_cast<size_t>(megabytes) * 1024 * 1024 / sizeof(TTSlot);
    size_t slots = 1;
    while (slots <= capacity / 2) slots *= 2;
    auto replacement = std::make_unique<TTSlot[]>(slots);
    table = std::move(replacement);
    size = slots;
    age = 0;
}

void TransTable::put(TTEntry entry, u64 hash) {
    size_t idx = (hash & (size-1));

    const TTEntry cur_entry = table[idx].load();
    
    bool replace = false;

    if (!cur_entry.is_valid()) {
        replace = true;
    } else {
        if (cur_entry.age < this->age) {
            replace = true;
        } else if (cur_entry.get_depth() < entry.get_depth()) {
            replace = true;
        } else if ((cur_entry.key ^ cur_entry.data) == hash &&
                   cur_entry.get_depth() == entry.get_depth() &&
                   (entry.get_type() == EXACT || cur_entry.get_type() != EXACT)) {
            // An aspiration re-search can improve the bound or find a new PV
            // at the same depth. Preserve an existing exact result over a bound.
            replace = true;
        }
    }

    if (!replace) return;

    entry.age = this->age;
    table[idx].store(entry);
}

TTEntry TransTable::get(u64 hash) const {
    size_t idx = (hash & (size-1));
    const TTEntry entry = table[idx].load();

    u64 key = hash ^ entry.data;

    if (entry.is_valid() && key == entry.key) {
        return entry;
    }

    return TTEntry();
}

Move TransTable::get_pv_move(u64 hash) const {
    return this->get(hash).get_move();
}

void TransTable::init() {
    for (size_t i = 0; i < size; i++) {
        this->table[i].store(TTEntry());
    }
}

bool TTEntry::is_valid() const {
    return key != INVALID_KEY;
}

f32 TransTable::valid_ratio() const {
    u32 num_valid = 0;
    for (size_t i = 0; i < size; i++) {
        if (table[i].key.load(std::memory_order_relaxed) != INVALID_KEY)
            num_valid++;
    }

    return (f32)num_valid / (f32)size;
}
