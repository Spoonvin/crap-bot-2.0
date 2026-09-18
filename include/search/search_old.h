#pragma once

#include "chess/move/move.h"
#include "chess/game.h"
#include "chess/move/movegen.h"
#include "search/trans_table.h"
#include "search/opening_book.h"

#include <chrono>
#include <atomic>
#include <memory>

#define MAX_PLY 80

struct SearcherOld {

    u8 base_depth;

    Move root_move;

    u32 search_time;
    std::chrono::steady_clock::time_point deadline;

    bool stop_search;
    // Shared by all workers; reset by the caller before starting a new search.
    std::shared_ptr<std::atomic<bool>> cancel =
        std::make_shared<std::atomic<bool>>(false);
    unsigned int thread_count = 4;

    std::shared_ptr<TransTable> trans_table;
    OpeningBook book;

    Move killers[MAX_PLY];

    i32 node_count;

    public:

    SearcherOld(u8 depth);
    SearcherOld(u32 search_time);

    void set_search_time(u32 search_time);

    Move get_best_move(Game& game);
    Move get_best_move_parallel(Game& game);

    i32 alpha_beta(i32 alpha, i32 beta, u8 depth, u8 ply, Game& game, bool do_null);

    private:

    void iterative_deepening(Game& game);

    i32 quiescence(i32 alpha, i32 beta, u8 ply, Game& game);

    void mvv_lva_reordering(MoveList& moves, Move pv_move, u8 length, Game& game, u8 ply);

    // Returns true if we are past deadline
    bool check_deadline();

    i32 probe_trans_table(u64 hash, u8 depth, i32 alpha, i32 beta, u8 ply);
    void record_trans_table(u64 hash, u8 depth, Move move, i32 score, TTType type, u8 ply);

};
