#include "search/search.h"
#include "chess/game.h"
#include "search/evaluation.h"
#include "chess/move/movegen.h"
#include "chess/move/move.h"
#include "search/trans_table.h"
#include "search/zobrist_hash.h"

#include <limits>
#include <chrono>
#include <iostream>
#include <thread>


struct MoveMvvLvaScore{
    Move move;
    i32 score;
};

Searcher::Searcher(u8 depth) {
    base_depth = depth;
    stop_search = false;

    trans_table.init();
}

Searcher::Searcher(u32 search_time) {
    this->search_time = search_time;
    stop_search = false;

    trans_table.init();
}

Searcher::~Searcher() {
    delete[] trans_table.table;
}

i32 Searcher::alpha_beta(i32 alpha, i32 beta, u8 depth, u8 ply, Game& game) {

    if (stop_search) return 0;

    // Check deadline every 4th ply (for performance)
    if ((ply & 0b111) == 0b101) {
        if (check_deadline()) return 0;
    }

    if (game.is_draw()) {
        return 0;
    }

    // Transposition table lookup
    i32 tt_val = probe_trans_table(game.hash, depth, alpha, beta);
    if (tt_val != UNKNOWN_TT_VALUE) {
        return tt_val;
    }

    // Track transposition table type
    TTType tt_type = UPPER;

    if (depth <= 0) return quiescence(alpha, beta, ply, game);

    i32 best_val = MIN_VALUE;
    Move best_move = Move::null();

    MoveList moves;
    GenResult gen_result = gen_legal(game, moves);

    if (gen_result.count <= 0) {
        if (gen_result.check == NO_CHECK) {
            return 0;
        } else {
            return -MATE_VALUE + ply;
        }
    }

    Move pv_move = this->trans_table.get_pv_move(game.hash);
    mvv_lva_reordering(moves, pv_move, gen_result.count, game);

    for (u8 i = 0; i < gen_result.count; ++i) {
        Move move = moves[i];
        Game branch_game = game;
        branch_game.make_move(move);

        i32 branch_val = -alpha_beta(-beta, -alpha, depth-1, ply+1, branch_game);

        if (stop_search)
            return 0;
        
        if (branch_val > best_val) {
            best_val = branch_val;
            best_move = move;

            if (branch_val > alpha) {
                tt_type = EXACT;
                alpha = branch_val;
            }
        }

        if(branch_val >= beta) {
            record_trans_table(game.hash, depth, best_move, beta, LOWER);
            return beta;
        }
    }

    record_trans_table(game.hash, depth, best_move, alpha, tt_type);

    return alpha;
}

bool Searcher::check_deadline() {

    if (std::chrono::steady_clock::now() < deadline) return false;

    stop_search = true;
    return true;
}

Move Searcher::get_best_move(Game& game) {
    
    stop_search = false;
    deadline = std::chrono::steady_clock::now()
         + std::chrono::milliseconds(search_time);

    u8 iter_depth = 1;

    while (std::chrono::steady_clock::now() < deadline) {

        alpha_beta(MIN_VALUE, MAX_VALUE, iter_depth, 0, game);

        iter_depth++;
    }

    this->trans_table.age++;

    std::cout << "Search completed. Depth reached: " << (iter_depth - 1) << "\n";
    std::cout << "Nodes searched: " << this->node_count << "\n";

    return this->trans_table.get_pv_move(game.hash);
}


i32 Searcher::quiescence(i32 alpha, i32 beta, u8 ply, Game& game) {

    if (game.is_draw()) {
        return 0;
    }

    i32 static_eval = eval_game(game);
    i32 best_val = static_eval;

    if (ply > MAX_PLY) return static_eval;

    if (best_val >= beta){
        return beta;
    }
    if (best_val > alpha){
        alpha = best_val;
    }

    MoveList moves;
    GenResult gen_result = gen_non_quiet(game, moves);

    if (gen_result.count <= 0) {
        if (gen_result.check == NO_CHECK) {
            return best_val;
        }
        return -MATE_VALUE + ply;
    }

    mvv_lva_reordering(moves, Move::null(), gen_result.count, game);

    for (u8 i = 0; i < gen_result.count; ++i) {
        Move move = moves[i];
        Game branch_game = game;
        branch_game.make_move(move);

        i32 branch_val = -quiescence(-beta, -alpha, ply+1, branch_game);

        if (branch_val >= beta) {
            return branch_val;
        }
        if (branch_val > best_val) {
            best_val = branch_val;
        }
        if (branch_val > alpha) {
            alpha = branch_val;
        }
    }

    return best_val;
}

void Searcher::mvv_lva_reordering(MoveList& moves, Move pv_move, u8 length, Game& game) {
    MoveMvvLvaScore move_scores[length];

    for (u8 i = 0; i < length; i++) {
        Move move = moves[i];
        i32 move_score = (move.data == pv_move.data) ? 
            MAX_VALUE : mvv_lva_score(move, game);

        move_scores[i] = {move, move_score};
    }

    // Insertion sort (descending score)
    for (u8 i = 1; i < length; i++) {
        MoveMvvLvaScore key = move_scores[i];
        u8 j = i;
        while (j > 0 && move_scores[j - 1].score < key.score) {
            move_scores[j] = move_scores[j - 1];
            j--;
        }
        move_scores[j] = key;
    }

    for (u8 i = 0; i < length; i++) {
        moves[i] = move_scores[i].move;
    }
}

i32 Searcher::probe_trans_table(u64 hash, u8 depth, i32 alpha, i32 beta) {

    TTEntry entry = trans_table.get(hash);

    if (entry.is_valid()) {

        if (entry.get_depth() >= depth) {

            TTType type = entry.get_type();
            i32 score = entry.get_score();

            if (type == EXACT)

                return score;

            if ((type == UPPER) && (score <= alpha))

                return alpha;

            if ((type == LOWER) && (score >= beta))

                return beta;

        }

    }

    return UNKNOWN_TT_VALUE;
}

void Searcher::record_trans_table(u64 hash, u8 depth, Move move, i32 score, TTType type) {
    TTEntry entry = TTEntry(hash, move, depth, score, type);
    trans_table.put(entry, hash);
}

void thread_search(Searcher* searcher, Game game) {

    u8 iter_depth = 1;

    while (std::chrono::steady_clock::now() < searcher->deadline) {

        searcher->alpha_beta(MIN_VALUE, MAX_VALUE, iter_depth, 0, game);

        iter_depth++;
    }

    std::cout << "Depth: " << (int)iter_depth - 1 << "\n";

    return;
}

Move Searcher::get_best_move_parallel(Game& game) {

    stop_search = false;
    deadline = std::chrono::steady_clock::now()
         + std::chrono::milliseconds(search_time);

    std::vector<std::thread> threads;

    for (int i = 0; i < 3; i++) {
        threads.emplace_back(thread_search, this, game);
    }

    u8 iter_depth = 1;

    while (std::chrono::steady_clock::now() < this->deadline) {

        alpha_beta(MIN_VALUE, MAX_VALUE, iter_depth, 0, game);

        iter_depth++;
    }

    for (auto& t : threads) {
        t.join();
    }

    return this->trans_table.get_pv_move(game.hash);
}