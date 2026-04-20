#include "search/search.h"
#include "chess/game.h"
#include "search/evaluation.h"
#include "chess/move/movegen.h"
#include "chess/move/move.h"

#include <omp.h>
#include <limits>
#include <chrono>

#define MAX_PLY 30

struct MoveMvvLvaScore{
    Move move;
    i32 score;
};

Searcher::Searcher(Game& game, u8 depth) {
    base_game = game;
    base_depth = depth;
    best_move.data = 0;
    stop_search = false;
}

Searcher::Searcher(Game& game, u32 search_time) {
    base_game = game;
    this->search_time = search_time;
    best_move.data = 0;
    stop_search = false;
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
                alpha = branch_val;
            }
        }

        if(branch_val >= beta) {
            break;
        }
    }

    return best_val;
}

Move Searcher::get_best_move() {
    alpha_beta(MIN_VALUE, MAX_VALUE, base_depth, 0, base_game);
    return best_move;
}

bool Searcher::check_deadline() {

    if (std::chrono::steady_clock::now() < deadline) return false;

    stop_search = true;
    return true;
}

Move Searcher::get_best_move_iterative() {
    
    deadline = std::chrono::steady_clock::now()
         + std::chrono::milliseconds(search_time);

    u8 start_depth = 1;

    MoveList moves;
    GenResult gen_result = gen_legal(base_game, moves);

    while (std::chrono::steady_clock::now() < deadline) {

        Move current_best_move = Move::null();
        i32 alpha = MIN_VALUE;
        i32 beta = MAX_VALUE;

        for (u8 i = 0; i < gen_result.count; ++i) {

            Move move = moves[i];
            Game branch_game = base_game;
            branch_game.make_move(move);

            i32 branch_val = -alpha_beta(-beta, -alpha, start_depth, 1, branch_game);

            if (stop_search) break;

            if (branch_val > alpha) {
                alpha = branch_val;
                current_best_move = move;
            }
        }

        if (!stop_search) {
            best_move = current_best_move;
        }

        start_depth++;
    }

    return best_move;
}

Move get_best_move_iterative(Game& game, u32 search_time) {
    Searcher searcher(game, search_time);
    return searcher.get_best_move_iterative();
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


Move get_best_move(Game& game, u8 depth) {
    Searcher searcher(game, depth);
    return searcher.get_best_move();
}