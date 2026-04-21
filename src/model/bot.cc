#include "model/bot.h"
#include "search/search.h"

#define DEPTH 4

Bot::Bot() : searcher(Searcher(u32(2000))) {}

Move Bot::select_best(Game& game) {
    return searcher.get_best_move_iterative(game);
}