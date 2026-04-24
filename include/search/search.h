#pragma once
#include "chess/move/move.h"
#include "chess/game.h"
#include "chess/move/movegen.h"
#include "search/trans_table.h"

#include <chrono>

#define MAX_PLY 50

struct Searcher{

    u8 base_depth;

    u32 search_time;
    std::chrono::steady_clock::time_point deadline;

    bool stop_search;

    TransTable trans_table;

    i32 node_count;

    public:

    Searcher(u8 depth);
    Searcher(u32 search_time);

    ~Searcher();

    Move get_best_move(Game& game);

    private:

    i32 alpha_beta(i32 alpha, i32 beta, u8 depth, u8 ply, Game& game);

    i32 quiescence(i32 alpha, i32 beta, u8 ply, Game& game);

    void mvv_lva_reordering(MoveList& moves, u8 length, Game& game);

    // Returns true if we are past deadline
    bool check_deadline();

    i32 probe_trans_table(u64 hash, u8 depth, i32 alpha, i32 beta);
    void record_trans_table(u64 hash, u8 depth, Move move, i32 score, TTType type);

};

struct ThreadData {
    Game game;
    Searcher searcher;
};