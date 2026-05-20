#include "model/bot.h"
#include "search/search.h"

#define DEPTH 4
#define TIME_LIM 800


Bot::Bot() : searcher((u32)TIME_LIM) {}

Bot::Bot(u32 time_lim) : searcher(time_lim) {}

Bot::~Bot() {
    delete this->searcher.trans_table;
}

Move Bot::select_best(Game& game) {
    return searcher.get_best_move_parallel(game);
}