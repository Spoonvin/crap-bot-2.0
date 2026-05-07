#pragma once

#include "common/types.h"

// Needs to be power of 2
#define HISTORY_SIZE 4096

struct HistoryEntry {
    i32 count;
    u64 hash;
};

struct History {
    HistoryEntry entries[HISTORY_SIZE];

    History();

    void track(u64 hash);
    void untrack(u64 hash);

    bool is_3f_repitition(u64 hash);
};