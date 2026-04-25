#include "model/bot.h"
#include "search/search.h"

#define DEPTH 4
#define TIME_LIM 6000


Bot::Bot() : searcher((u32)TIME_LIM) {}

Bot::Bot(u32 time_lim) : searcher(time_lim) {}


Move Bot::select_best(Game& game) {
    return searcher.get_best_move(game);
}