#include "search/search.h"
#include "search/zobrist_hash.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <string>

namespace {

Searcher* timed_searcher = nullptr;
i32 expire_at_node = 0;
std::function<bool()> expire_on_clock;

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

// Advance the clock at a chosen node, without sleeps or a racing stop thread.
struct ExpireAfterNodes {
    ExpireAfterNodes(Searcher& searcher, i32 nodes) {
        timed_searcher = &searcher;
        expire_at_node = nodes;
    }

    ~ExpireAfterNodes() {
        timed_searcher = nullptr;
    }
};

void reset(Searcher& searcher) {
    searcher.trans_table->init();
    searcher.root_move = Move::null();
    searcher.stop_search = false;
    searcher.node_count = 0;
    searcher.deadline = std::chrono::steady_clock::now() + std::chrono::hours(1);
    std::fill(std::begin(searcher.killers), std::end(searcher.killers), Move::null());
}

std::string fen(Game& game) {
    char buffer[MAX_FEN];
    game.store_fen(buffer);
    return buffer;
}

void cache_child(Searcher& searcher, Game& game, Move move, i32 root_score,
                 TTType type = EXACT) {
    game.make_move(move);
    searcher.trans_table->put(TTEntry(game.hash, Move::null(), 8, -root_score, type),
                             game.hash);
    game.unmake_move(move);
}

void test_same_depth_replacement(Searcher& searcher) {
    reset(searcher);
    auto& table = *searcher.trans_table;
    const u64 hash = 0x12345;
    const Move a = Move::normal(0, 1);
    const Move b = Move::normal(1, 2);

    table.put(TTEntry(hash, a, 5, 125, LOWER), hash);
    table.put(TTEntry(hash, b, 5, 180, EXACT), hash);
    TTEntry entry = table.get(hash);
    require(entry.get_move().data == b.data && entry.get_score() == 180 &&
            entry.get_type() == EXACT,
            "an exact aspiration re-search must replace the same-depth bound and PV");

    table.put(TTEntry(hash, a, 5, 200, UPPER), hash);
    require(table.get(hash).get_move().data == b.data,
            "a same-depth bound must not displace an exact result");
    table.put(TTEntry(hash, a, 4, 190, EXACT), hash);
    require(table.get(hash).get_move().data == b.data,
            "a shallower result must not displace a deeper result");

    const u64 collision = hash + TT_SIZE;
    table.put(TTEntry(collision, a, 5, 250, EXACT), collision);
    require(table.get(hash).get_move().data == b.data,
            "same-depth replacement must match the position, not just the table index");

    ++table.age;
    table.put(TTEntry(hash, a, 5, 125, LOWER), hash);
    table.put(TTEntry(hash, b, 5, 175, LOWER), hash);
    require(table.get(hash).get_move().data == b.data &&
            table.get(hash).get_score() == 175,
            "a new same-depth bound must be able to refresh its move and score");
}

void test_saved_pv_first(Searcher& searcher) {
    reset(searcher);
    Game game = Game::initial();
    const Move previous = Move::normal(1, 16); // b1a3, normally behind pawn moves
    const Move stale = Move::normal(8, 16);   // a2a3
    searcher.root_move = previous;
    searcher.trans_table->put(TTEntry(game.hash, stale, 0, 0, UPPER), game.hash);
    cache_child(searcher, game, previous, 100);
    cache_child(searcher, game, stale, 100);

    const i32 score = searcher.alpha_beta(-25, 25, 1, 0, game, false);
    require(score == 25 && searcher.root_move.data == previous.data,
            "the saved PV must be searched before a stale TT move");
    require(searcher.node_count == 2, "the saved PV must cause the first-move cutoff");
}

void test_root_bound_does_not_select_move(Searcher& searcher) {
    reset(searcher);
    Game game = Game::initial();
    const Move previous = Move::normal(1, 16);
    const Move stale = Move::normal(8, 16);
    searcher.root_move = previous;
    searcher.trans_table->put(TTEntry(game.hash, stale, 8, 25, LOWER), game.hash);
    cache_child(searcher, game, previous, 100);

    const i32 score = searcher.alpha_beta(-25, 25, 1, 0, game, false);
    require(score == 25 && searcher.root_move.data == previous.data &&
            searcher.node_count == 2,
            "a root TT bound must not bypass the search and replace the saved PV");
}

void test_fail_low_preserves_move(Searcher& searcher) {
    reset(searcher);
    Game game = Game::initial();
    const Move previous = Move::normal(1, 16);
    searcher.root_move = previous;

    MoveList moves;
    const GenResult generated = gen_legal(game, moves);
    for (u8 i = 0; i < generated.count; ++i) {
        // The saved move scores -100. Other children only establish <= -50
        // from the root's perspective; they could actually be much worse.
        if (moves[i].data == previous.data)
            cache_child(searcher, game, moves[i], -100);
        else
            cache_child(searcher, game, moves[i], -50, LOWER);
    }

    const i32 score = searcher.alpha_beta(-50, 50, 1, 0, game, false);
    require(score == -50 && searcher.root_move.data == previous.data,
            "larger fail-low bounds must not replace the saved move");
}

void test_interrupted_root_search(Searcher& searcher, bool improvement) {
    reset(searcher);
    Game game = Game::initial();
    const std::string before = fen(game);
    const u64 hash = game.hash;
    const i32 stack_size = game.state_stack.size;
    const Move previous = Move::normal(1, 16);
    const Move better = Move::normal(8, 16);
    searcher.root_move = previous;

    if (improvement) {
        cache_child(searcher, game, previous, -100);
        cache_child(searcher, game, better, -50);
    }

    // Cached root children finish before the next branch reaches a time check.
    ExpireAfterNodes expiration(searcher, improvement ? 4 : 2);
    searcher.alpha_beta(-1000, 1000, 4, 0, game, false);

    require(searcher.stop_search, "the search must stop inside an uncached child");
    require(searcher.root_move.data == (improvement ? better : previous).data,
            "interruption must preserve a completed improvement or the previous PV");
    require(fen(game) == before, "interruption must restore the original FEN");
    require(game.hash == hash, "interruption must restore the original hash");
    require(game.state_stack.size == stack_size,
            "interruption must restore the original state stack size");
    require(!searcher.trans_table->get(game.hash).is_valid(),
            "an interrupted root search must not record a completed result");
}

void test_interrupted_quiescence(Searcher& searcher) {
    reset(searcher);
    Game game = Game::from_fen("4k3/8/8/3p4/3Q4/8/8/4K3 w - - 0 1");
    const std::string before = fen(game);
    const u64 hash = game.hash;

    ExpireAfterNodes expiration(searcher, 3);
    const i32 score = searcher.alpha_beta(MIN_VALUE, MAX_VALUE, 0, 3, game, false);
    require(searcher.stop_search && score == 0,
            "an interrupted quiescence child must propagate the stop before using its score");
    require(fen(game) == before && game.hash == hash && game.state_stack.size == 0,
            "interrupted quiescence must undo its captures");
}

void test_zero_time_and_terminal_positions(Searcher& searcher) {
    reset(searcher);
    searcher.set_search_time(0);
    for (bool parallel : {false, true}) {
        Game game = Game::from_fen("4k3/7p/8/8/8/8/P7/4K3 w - - 0 1");
        MoveList moves;
        require(gen_legal(game, moves).count > 0, "fallback test needs legal moves");
        require(searcher.book.lookup_position(game).is_null(),
                "fallback test must search outside the opening book");
        searcher.root_move = Move::normal(0, 63); // Stale move from another position.
        const Move result = parallel ? searcher.get_best_move_parallel(game)
                                     : searcher.get_best_move(game);
        require(result.data == moves[0].data,
                "zero-time searches must return a legal fallback, not a stale or null move");

        game = Game::from_fen("7k/6Q1/6K1/8/8/8/8/8 b - - 0 1");
        const Move terminal = parallel ? searcher.get_best_move_parallel(game)
                                       : searcher.get_best_move(game);
        require(terminal.is_null(), "a root with no legal moves must return a null move");
    }
}

void test_interrupted_aspiration_retry(Searcher& searcher) {
    reset(searcher);
    searcher.set_search_time(60000);
    Game game = Game::from_fen("4k3/7p/8/8/8/8/P7/4K3 w - - 0 1");
    require(searcher.book.lookup_position(game).is_null(),
            "aspiration test must search outside the opening book");
    MoveList moves;
    const GenResult generated = gen_legal(game, moves);
    require(generated.count > 1, "aspiration test needs multiple root moves");
    const Move previous = moves[0];
    const Move better = moves[generated.count - 1];
    const u64 root_hash = game.hash;

    // Both initial iterations score zero and keep the initial root choice.
    for (u8 i = 0; i < generated.count; ++i) {
        game.make_move(moves[i]);
        searcher.trans_table->put(TTEntry(game.hash, Move::null(), 1, 0, EXACT),
                                 game.hash);
        game.unmake_move(moves[i]);
    }

    bool changed_scores = false;
    expire_on_clock = [&] {
        TTEntry entry = searcher.trans_table->get(root_hash);
        if (entry.is_valid() && entry.get_depth() == 2 && !changed_scores) {
            // At depth 3 the previous move fails low and another fails high.
            require(game.hash == root_hash, "score changes must happen between root searches");
            for (u8 i = 0; i < generated.count; ++i)
                cache_child(searcher, game, moves[i],
                            moves[i].data == better.data ? 100 : -100);
            changed_scores = true;
        }
        return entry.is_valid() && entry.get_depth() == 3 && entry.get_type() == LOWER;
    };
    timed_searcher = &searcher;
    expire_at_node = std::numeric_limits<i32>::max();

    const Move result = searcher.get_best_move(game);
    expire_on_clock = {};
    timed_searcher = nullptr;

    require(changed_scores && searcher.stop_search,
            "the search must stop during the depth-3 aspiration retry");
    require(searcher.trans_table->get(root_hash).get_move().data == better.data,
            "the failed aspiration attempt must have found a different candidate");
    require(result.data == previous.data,
            "an interrupted retry without an improvement must retain the completed iteration");
}

} // namespace

extern "C" std::chrono::steady_clock::time_point
__real__ZNSt6chrono3_V212steady_clock3nowEv();

extern "C" std::chrono::steady_clock::time_point
__wrap__ZNSt6chrono3_V212steady_clock3nowEv() {
    if (timed_searcher && ((expire_on_clock && expire_on_clock()) ||
                          timed_searcher->node_count >= expire_at_node))
        return timed_searcher->deadline + std::chrono::milliseconds(1);
    return __real__ZNSt6chrono3_V212steady_clock3nowEv();
}

int main() {
    init_hash_key_map();
    Searcher searcher(u32{0});
    test_same_depth_replacement(searcher);
    test_saved_pv_first(searcher);
    test_root_bound_does_not_select_move(searcher);
    test_fail_low_preserves_move(searcher);
    test_interrupted_root_search(searcher, false);
    test_interrupted_root_search(searcher, true);
    test_interrupted_quiescence(searcher);
    test_interrupted_aspiration_retry(searcher);
    test_zero_time_and_terminal_positions(searcher);
    std::cout << "Search regression tests passed\n";
}
