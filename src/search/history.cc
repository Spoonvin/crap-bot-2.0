#include <cassert>

#include "search/history.h"

History::History() {
    for (u32 i = 0; i < HISTORY_SIZE; i++) {
        this->entries[i].count = 0;
        this->entries[i].hash = 0;
    }
}

void History::track(u64 hash) {
    u32 idx = (hash & (HISTORY_SIZE-1));

    HistoryEntry entry = this->entries[idx];

    // Replace if collision
    if (entry.hash != hash) {
        entry.hash = hash;
        entry.count = 1;
    } else {
        entry.count += 1;
    }

    this->entries[idx] = entry;
}

void History::untrack(u64 hash) {
    u32 idx = (hash & (HISTORY_SIZE-1));
    HistoryEntry entry = this->entries[idx];

    if (entry.hash != hash)
        return;

    if (entry.count > 0)
        entry.count -= 1;
    
    this->entries[idx] = entry;
}

bool History::is_3f_repitition(u64 hash) {
    u32 idx = (hash & (HISTORY_SIZE-1));
    HistoryEntry entry = this->entries[idx];

    return entry.count >= 3;
}