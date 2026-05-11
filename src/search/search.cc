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

#define BOOK_PATH "/home/spoonvin/projects/chess-parallel/assets/Book.txt"

#define KILLER_BONUS 300

struct MoveMvvLvaScore{
    Move move;
    i32 score;
};

Searcher::Searcher(u8 depth) : book(BOOK_PATH) {

    base_depth = depth;
    stop_search = false;
}

Searcher::Searcher(u32 search_time) : book(BOOK_PATH) {
    this->search_time = search_time;
    stop_search = false;

    trans_table.init();
}

Searcher::~Searcher() {
    delete[] trans_table.table;
}

i32 Searcher::alpha_beta(i32 alpha, i32 beta, u8 depth, u8 ply, Game& game, bool do_null) {

    this->node_count++;

    if (stop_search) return 0;

    // Check deadline only for some plys (for performance)
    if ((ply & 0b111) == 0b100) {
        if (check_deadline()) return 0;
    }

    if (game.is_draw()) {
        return 0;
    }

    // Transposition table lookup
    i32 tt_val = probe_trans_table(game.hash, depth, alpha, beta);
    if (tt_val != UNKNOWN_TT_VALUE) {
        if (ply == 0)
            this->root_move = trans_table.get_pv_move(game.hash);
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

    Move pv_move = this->trans_table.get_pv_move(game.hash);
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

            if (ply == 0)
                this->root_move = move;

            if (branch_val > alpha) {
                tt_type = EXACT;
                alpha = branch_val;
            }
        }

        if(branch_val >= beta) {
            record_trans_table(game.hash, depth, best_move, beta, LOWER);

            // Homemode killer heuristic
            // If move was searched late and caused cutoff ->
            // store and give move order bonus later
            if (i > (gen_result.count >> 2)) {
                killers[ply] = move;
            }

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

    Move book_move = this->book.lookup_position(game);
    if (!book_move.is_null())
        return book_move;
    
    this->root_move = Move::null();
    stop_search = false;
    deadline = std::chrono::steady_clock::now()
         + std::chrono::milliseconds(search_time);

    u8 iter_depth = 1;
    i32 prev_score = 0;

    while (std::chrono::steady_clock::now() < deadline) {

        i32 window = 25;
        i32 alpha = MIN_VALUE;
        i32 beta = MAX_VALUE;

        if (iter_depth > 2) {
            alpha = prev_score - window;
            beta = prev_score + window;
        }

        i32 score = 0;

        while (true) {
            score = alpha_beta(alpha, beta, iter_depth, 0, game, true);

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

        prev_score = score;

        iter_depth++;

    }

    this->trans_table.age++;

    //std::cout << "Node searched: " << node_count << "\n";
    this->node_count = 0;

    Move final_move = this->root_move;
    assert(!final_move.is_null());

    return final_move;
}

i32 Searcher::quiescence(i32 alpha, i32 beta, u8 ply, Game& game) {

    this->node_count++;

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

    mvv_lva_reordering(moves, Move::null(), gen_result.count, game, ply);

    for (u8 i = 0; i < gen_result.count; ++i) {
        Move move = moves[i];
        game.make_move(move);

        i32 branch_val = -quiescence(-beta, -alpha, ply+1, game);
        game.unmake_move(move);

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

        searcher->alpha_beta(MIN_VALUE, MAX_VALUE, iter_depth, 0, game, true);

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

        alpha_beta(MIN_VALUE, MAX_VALUE, iter_depth, 0, game, true);

        iter_depth++;
    }

    for (auto& t : threads) {
        t.join();
    }

    this->trans_table.age++;

    //std::cout << "Valid ratio: " << this->trans_table.valid_ratio() << "\n";

    return this->trans_table.get_pv_move(game.hash);
}