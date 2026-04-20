#pragma once
#include "chess/move/move.h"
#include "chess/game.h"
#include "chess/move/movegen.h"

#include <chrono>

Move get_best_move(Game& game, u8 depth);
Move get_best_move_iterative(Game& game, u32 search_time);

struct Searcher{
    Game base_game;

    Move best_move;

    u8 base_depth;

    u32 search_time;
    std::chrono::steady_clock::time_point deadline;

    bool stop_search;

    public:

    Searcher(Game& game, u8 depth);
    Searcher(Game& game, u32 search_time);

    Move get_best_move();
    Move get_best_move_iterative();

    private:

    i32 alpha_beta(i32 alpha, i32 beta, u8 depth, u8 ply, Game& game);

    i32 quiescence(i32 alpha, i32 beta, u8 ply, Game& game);

    void mvv_lva_reordering(MoveList& moves, u8 length, Game& game);

    // Returns true if we are past deadline
    bool check_deadline();
};