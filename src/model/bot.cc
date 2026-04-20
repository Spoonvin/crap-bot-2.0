#include "model/bot.h"
#include "search/search.h"

#define DEPTH 4

Move Bot::select_best(Game& game) {
    return get_best_move_iterative(game, 5000);
}