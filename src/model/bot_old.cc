#include "model/bot_old.h"
#include "search/search_old.h"

#define DEPTH 4
#define TIME_LIM 400


BotOld::BotOld() : searcher((u32)TIME_LIM) {}

BotOld::BotOld(u32 time_lim) : searcher(time_lim) {}

Move BotOld::select_best(Game& game) {
    return searcher.get_best_move(game);
}

void BotOld::clear_transposition_table() {
    searcher.trans_table->init();
    searcher.trans_table->age = 0;
}
