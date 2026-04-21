#include "search/search.h"
#include "chess/game.h"
#include "search/evaluation.h"
#include "chess/move/movegen.h"
#include "chess/move/move.h"
#include "search/trans_table.h"
#include "search/zobrist_hash.h"

#include <omp.h>
#include <limits>
#include <chrono>
#include <iostream>

#define MAX_PLY 30

struct MoveMvvLvaScore{
    Move move;
    i32 score;
};

Searcher::Searcher(u8 depth) {
    base_depth = depth;
    best_move.data = 0;
    stop_search = false;

    trans_table.init();
}

Searcher::Searcher(u32 search_time) {
    this->search_time = search_time;
    best_move.data = 0;
    stop_search = false;

    trans_table.init();
}

i32 Searcher::alpha_beta(i32 alpha, i32 beta, u8 depth, u8 ply, Game& game) {

    if (stop_search) return 0;

    // Check deadline every 4th ply (for performance)
    if ((ply & 0b11) == 0b11) {
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

    MoveList moves;
    GenResult gen_result = gen_legal(game, moves);

    if (gen_result.count <= 0) {
        if (gen_result.check == NO_CHECK) {
            return 0;
        } else {
            return -MATE_VALUE + ply;
        }
    }

    mvv_lva_reordering(moves, gen_result.count, game);

    for (u8 i = 0; i < gen_result.count; ++i) {
        Move move = moves[i];
        Game branch_game = game;
        branch_game.make_move(move);

        i32 branch_val = -alpha_beta(-beta, -alpha, depth-1, ply+1, branch_game);

        if (branch_val > best_val) {
            best_val = branch_val;
            if (ply == 0) best_move = move;

            if (branch_val > alpha) {
                tt_type = EXACT;
                alpha = branch_val;
            }
        }

        if(branch_val >= beta) {
            if (!stop_search)
                record_trans_table(game.hash, depth, beta, LOWER);
            return beta;
        }
    }

    if (!stop_search)
        record_trans_table(game.hash, depth, alpha, tt_type);

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

    MoveList moves;
    GenResult gen_result = gen_legal(game, moves);

    while (std::chrono::steady_clock::now() < deadline) {

        Move current_best_move = Move::null();
        i32 alpha = MIN_VALUE;
        i32 beta = MAX_VALUE;

        for (u8 i = 0; i < gen_result.count; ++i) {

            Move move = moves[i];
            Game branch_game = game;
            branch_game.make_move(move);

            i32 branch_val = -alpha_beta(-beta, -alpha, iter_depth, 1, branch_game);

            if (stop_search) break;

            if (branch_val > alpha) {
                alpha = branch_val;
                current_best_move = move;
            }
        }

        if (!stop_search) {
            best_move = current_best_move;
        }

        iter_depth++;
    }

    return best_move;
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

    mvv_lva_reordering(moves, gen_result.count, game);

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

void Searcher::mvv_lva_reordering(MoveList& moves, u8 length, Game& game) {
    MoveMvvLvaScore move_scores[length];

    for (u8 i = 0; i < length; i++) {
        Move move = moves[i];
        move_scores[i] = {move, mvv_lva_score(move, game)};
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

    if (hash == entry.hash && entry.valid) {
        if (entry.depth >= depth) {

            if (entry.type == EXACT)

                return entry.score;

            if ((entry.type == UPPER) && (entry.score <= alpha))

                return alpha;

            if ((entry.type == LOWER) && (entry.score >= beta))

                return beta;

        }

    }

    return UNKNOWN_TT_VALUE;
}

void Searcher::record_trans_table(u64 hash, u8 depth, i32 score, TTType type) {
    TTEntry entry = TTEntry(hash, Move::null(), depth, score, type);
    trans_table.put(entry);
}