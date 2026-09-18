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

#define BOOK_PATH "/home/edvin/projects/crap-bot-2.0/assets/Book.txt"

#define KILLER_BONUS 300

struct MoveMvvLvaScore{
    Move move;
    i32 score;
};

namespace {

constexpr i32 MATE_SCORE_THRESHOLD = MATE_VALUE - MAX_PLY;

i32 score_to_tt(i32 score, u8 ply) {
    if (score >= MATE_SCORE_THRESHOLD) return score + ply;
    if (score <= -MATE_SCORE_THRESHOLD) return score - ply;
    return score;
}

i32 score_from_tt(i32 score, u8 ply) {
    if (score >= MATE_SCORE_THRESHOLD) return score - ply;
    if (score <= -MATE_SCORE_THRESHOLD) return score + ply;
    return score;
}

}


Searcher::Searcher(u8 depth)
    : base_depth(depth), root_move(Move::null()), search_time(0),
      stop_search(false), trans_table(std::make_shared<TransTable>()),
      book(BOOK_PATH), killers{}, node_count(0) {}

Searcher::Searcher(u32 search_time)
    : base_depth(0), root_move(Move::null()), search_time(search_time),
      stop_search(false), trans_table(std::make_shared<TransTable>()),
      book(BOOK_PATH), killers{}, node_count(0) {}

void Searcher::set_search_time(u32 search_time) {
    this->search_time = search_time;
}

void Searcher::set_thread_id(i32 thread_id) {
    this->thread_id = thread_id;
}

i32 Searcher::alpha_beta(i32 alpha, i32 beta, u8 depth, u8 ply, Game& game, bool do_null) {

    this->node_count++;

    if (stop_search) return 0;

    // Check deadline only for some plys (for performance)
    if (ply == 0 || (ply & 0b111) == 0b100) {
        if (check_deadline()) return 0;
    }

    if (game.is_draw()) {
        return 0;
    }

    // Check extensions can exceed the iteration depth. Stop before indexing
    // killers[ply] or recursing beyond the supported mate-distance range.
    if (ply >= MAX_PLY) return eval_game(game);

    if (ply > 0) {
        i32 tt_val = probe_trans_table(game.hash, depth, alpha, beta, ply);
        if (tt_val != UNKNOWN_TT_VALUE)
            return tt_val;
    }

    if (depth <= 0) return quiescence(alpha, beta, ply, game);

    // Track transposition table type
    TTType tt_type = UPPER;
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

    // Probes use the requested depth, before this node's check extension.
    // Store the same depth so this result cannot satisfy a deeper request.
    const u8 tt_depth = depth;
    if (gen_result.check != NO_CHECK)
        depth++;
    
    if (do_null && (gen_result.check == NO_CHECK) && (ply > 0) && (depth >= 3) &&
        game.player_has_non_pawn_piece()) {

        int R = 2 + depth / 4;

        game.make_null_move();
        i32 score = -alpha_beta(-beta, -beta+1, depth-1-R, ply+1, game, false);
        game.unmake_null_move();

        if (this->stop_search)
            return 0;

        if (score >= beta && abs(score) < MATE_VALUE) {
			return beta;
		}
    }

    Move pv_move = (ply == 0 && !root_move.is_null())
        ? root_move : trans_table->get_pv_move(game.hash);
    mvv_lva_reordering(moves, pv_move, gen_result.count, game, ply);

    for (u8 i = 0; i < gen_result.count; ++i) {
        Move move = moves[i];
        game.make_move(move);

        i32 branch_val = -alpha_beta(-beta, -alpha, depth-1, ply+1, game, do_null);
        game.unmake_move(move);

        if (stop_search)
            return 0;
        
        if (branch_val > best_val) {
            best_val = branch_val;
            best_move = move;

            if (branch_val > alpha) {
                // Only an alpha improvement can replace the saved root move;
                // fail-low results may be upper bounds, not comparable scores.
                if (ply == 0)
                    this->root_move = move;

                tt_type = EXACT;
                alpha = branch_val;
            }
        }

        if(branch_val >= beta) {
            record_trans_table(game.hash, tt_depth, best_move, beta, LOWER, ply);

            // Homemode killer heuristic
            // If move was searched late and caused cutoff ->
            // store and give move order bonus later
            if (i > (gen_result.count >> 2)) {
                killers[ply] = move;
            }

            return beta;
        }
    }

    record_trans_table(game.hash, tt_depth, best_move, alpha, tt_type, ply);

    return alpha;
}

i32 Searcher::pvs(i32 alpha, i32 beta, u8 depth, u8 ply, Game& game, bool do_null) {

    this->node_count++;

    if (stop_search) return 0;

    // Check deadline only for some plys (for performance)
    if (ply == 0 || (ply & 0b111) == 0b100) {
        if (check_deadline()) return 0;
    }

    if (game.is_draw()) {
        return 0;
    }

    // Check extensions can exceed the iteration depth. Stop before indexing
    // killers[ply] or recursing beyond the supported mate-distance range.
    if (ply >= MAX_PLY) return eval_game(game);

    if (ply > 0) {
        i32 tt_val = probe_trans_table(game.hash, depth, alpha, beta, ply);
        if (tt_val != UNKNOWN_TT_VALUE)
            return tt_val;
    }

    if (depth <= 0) return quiescence(alpha, beta, ply, game);

    // Track transposition table type
    TTType tt_type = UPPER;
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

    // Probes use the requested depth, before this node's check extension.
    // Store the same depth so this result cannot satisfy a deeper request.
    const u8 tt_depth = depth;
    if (gen_result.check != NO_CHECK)
        depth++;

    f32 eg_ratio = endgame_ratio(game);
    
    if (do_null && (gen_result.check == NO_CHECK) && (ply > 0) && (depth >= 4) &&
        game.player_has_non_pawn_piece()) {

        int R = 2 + depth / 3;

        game.make_null_move();
        i32 score = -pvs(-beta, -beta+1, depth-R, ply+1, game, false);
        game.unmake_null_move();

        if (this->stop_search)
            return 0;

        if (score >= beta && abs(score) < (MATE_VALUE - MAX_PLY)) {
			return beta;
		}
    }

    Move pv_move = (ply == 0 && !root_move.is_null())
        ? root_move : trans_table->get_pv_move(game.hash);
    mvv_lva_reordering(moves, pv_move, gen_result.count, game, ply);

    // For pvs
    // Always search first move with full window
    bool is_first_move = true;

    for (u8 i = 0; i < gen_result.count; ++i) {
        Move move = moves[i];
        game.make_move(move);

        i32 branch_val = 0;
        if (is_first_move) {
            // Full search
            branch_val = -pvs(-beta, -alpha, depth-1, ply+1, game, do_null);
        } else {
            // We assume the first move is best
            // Search rest with a narrow window
            branch_val = -pvs(-alpha-1, -alpha, depth-1, ply+1, game, do_null);

            // If move turns out to be better
            // do a full search
            if (!stop_search && (branch_val > alpha) && (branch_val < beta)) {
                branch_val = -pvs(-beta, -alpha, depth-1, ply+1, game, do_null);
            }
        }
        game.unmake_move(move);

        if (stop_search)
            return 0;
        
        if (branch_val > best_val) {
            best_val = branch_val;
            best_move = move;

            if (branch_val > alpha) {
                // Only an alpha improvement can replace the saved root move;
                // fail-low results may be upper bounds, not comparable scores.
                if (ply == 0)
                    this->root_move = move;

                tt_type = EXACT;
                alpha = branch_val;
            }
        }

        if(branch_val >= beta) {
            record_trans_table(game.hash, tt_depth, best_move, beta, LOWER, ply);

            // Homemode killer heuristic
            // If move was searched late and caused cutoff ->
            // store and give move order bonus later
            if (i > (gen_result.count >> 2)) {
                killers[ply] = move;
            }

            return beta;
        }

        is_first_move = false;
    }

    record_trans_table(game.hash, tt_depth, best_move, alpha, tt_type, ply);

    return alpha;
}

bool Searcher::check_deadline() {

    if (!cancel->load(std::memory_order_relaxed) &&
        std::chrono::steady_clock::now() < deadline) return false;

    stop_search = true;
    return true;
}

void Searcher::iterative_deepening(Game& game) {

    // Always have a legal fallback, even if the first iteration is interrupted.
    MoveList moves;
    GenResult generated = gen_legal(game, moves);
    root_move = generated.count > 0 ? moves[0] : Move::null();
    if (generated.count == 0 || game.is_draw())
        return;

    u8 iter_depth = 1;
    i32 prev_score = 0;

    while (!stop_search && iter_depth < MAX_PLY &&
           std::chrono::steady_clock::now() < deadline) {

        const Move completed_move = root_move;

        i32 window = 25;
        i32 alpha = MIN_VALUE;
        i32 beta = MAX_VALUE;

        if (iter_depth > 2) {
            alpha = prev_score - window;
            beta = prev_score + window;
        }

        i32 score = 0;

        while (true) {
            // Every aspiration attempt starts with the last completed PV.
            // If stopped, retain it unless this attempt found an improvement.
            root_move = completed_move;
            score = pvs(alpha, beta, iter_depth, 0, game, true);

            if (this->stop_search)
                break;

            if (score <= alpha) {
                alpha = MIN_VALUE;
            } else if (score >= beta) {
                beta = MAX_VALUE;
            } else {
                break;
            }
            
        }

        if (stop_search)
            break;

        prev_score = score;

        iter_depth++;

        // Spread out the threads
        if (thread_id != 0 && iter_depth > 2 &&
            ((iter_depth + thread_id) & 1)) {
            ++iter_depth;
        }

    }

    // Debug
    this->prev_eval = prev_score;
}

Move Searcher::get_best_move(Game& game) {

    Move book_move = this->book.lookup_position(game);
    if (!book_move.is_null())
        return book_move;

    stop_search = false;
    deadline = std::chrono::steady_clock::now()
         + std::chrono::milliseconds(search_time);

    iterative_deepening(game);

    this->trans_table->age++;

    //std::cout << "Node searched: " << node_count << "\n";
    this->node_count = 0;

    return root_move;
}

i32 Searcher::quiescence(i32 alpha, i32 beta, u8 ply, Game& game) {

    this->node_count++;

    if (stop_search) return 0;
    if ((ply & 0b111) == 0b100 && check_deadline()) return 0;

    if (game.is_draw()) {
        return 0;
    }

    i32 static_eval = eval_game(game);
    if (ply >= MAX_PLY) return static_eval;

    MoveList moves;
    GenResult gen_result = gen_non_quiet(game, moves);

    // In check, a legal evasion is mandatory. Standing pat must neither
    // cause a cutoff nor compete with the scores of the evasions.
    i32 best_val = MIN_VALUE;
    if (gen_result.check == NO_CHECK) {
        best_val = static_eval;
        if (best_val >= beta) return beta;
        if (best_val > alpha) alpha = best_val;
    }

    if (gen_result.count <= 0) {
        if (gen_result.check == NO_CHECK) {
            return best_val;
        }
        return -MATE_VALUE + ply;
    }

    mvv_lva_reordering(moves, Move::null(), gen_result.count, game, ply);

    for (u8 i = 0; i < gen_result.count; ++i) {
        Move move = moves[i];
        game.make_move(move);

        i32 branch_val = -quiescence(-beta, -alpha, ply+1, game);
        game.unmake_move(move);

        if (stop_search)
            return 0;

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

void Searcher::mvv_lva_reordering(MoveList& moves, Move pv_move, u8 length, Game& game, u8 ply) {
    MoveMvvLvaScore move_scores[length];

    for (u8 i = 0; i < length; i++) {
        Move move = moves[i];
        i32 move_score = (move.data == pv_move.data) ? 
            MAX_VALUE : mvv_lva_score(move, game);
        
        if (killers[ply].data == move.data)
            move_score += KILLER_BONUS;

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

i32 Searcher::probe_trans_table(u64 hash, u8 depth, i32 alpha, i32 beta, u8 ply) {

    const TTEntry entry = trans_table->get(hash);

    if (entry.is_valid()) {

        if (entry.get_depth() >= depth) {

            TTType type = entry.get_type();
            i32 score = score_from_tt(entry.get_score(), ply);

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

void Searcher::record_trans_table(u64 hash, u8 depth, Move move, i32 score, TTType type, u8 ply) {
    TTEntry entry = TTEntry(hash, move, depth, score_to_tt(score, ply), type);
    trans_table->put(entry, hash);
}

Move Searcher::get_best_move_parallel(Game& game) {

    Move book_move = this->book.lookup_position(game);
    if (!book_move.is_null())
        return book_move;

    stop_search = false;
    deadline = std::chrono::steady_clock::now()
         + std::chrono::milliseconds(search_time);

    std::vector<std::thread> threads;

    for (unsigned int i = 1; i < thread_count; i++) {
        threads.emplace_back([searcher = *this, game, i]() mutable {
            searcher.set_thread_id(i);
            searcher.iterative_deepening(game);
        });
    }

    iterative_deepening(game);

    for (auto& t : threads) {
        t.join();
    }

    // The generation is shared but non-atomic; all TT users have now stopped.
    this->trans_table->age++;

    //std::cout << "Node searched: " << node_count << "\n";
    this->node_count = 0;

    return root_move;
}
