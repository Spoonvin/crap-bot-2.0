#include "search/search.h"
#include "search/zobrist_hash.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <thread>

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

void test_hash_resize(Searcher& searcher) {
    auto& table = *searcher.trans_table;
    for (unsigned int megabytes : {1u, 3u, 8u}) {
        table.resize(megabytes);
        require(table.size * sizeof(TTSlot) <= megabytes * 1024 * 1024 &&
                (table.size & (table.size - 1)) == 0,
                "Hash must allocate a power-of-two table within its memory budget");
        const u64 hash = 0xfedcba9876543210ULL;
        table.put(TTEntry(hash, Move::normal(0, 1), 5, 125, EXACT), hash);
        require(table.get(hash).get_score() == 125,
                "resized tables must retain and probe entries using their new mask");
        table.init();
        require(!table.get(hash).is_valid() && table.valid_ratio() == 0,
                "clearing a resized table must remove its entries");
    }
}

void test_tt_snapshot_validation(Searcher& searcher) {
    reset(searcher);
    auto& table = *searcher.trans_table;
    const u64 hash = 0x12345;
    const TTEntry a(hash, Move::normal(0, 1), 5, -125, EXACT);
    const TTEntry b(hash, Move::normal(1, 2), 5, 250, LOWER);

    require(!table.get(hash).is_valid(), "a cleared atomic slot must be a miss");
    table.put(a, hash);
    const TTEntry saved = table.get(hash);
    require(saved.data == a.data && saved.get_score() == -125,
            "an atomic slot must preserve the complete packed payload");

    // Model a reader observing the data and key from different writes.
    table.table[hash & (TT_SIZE - 1)].key.store(b.key, std::memory_order_relaxed);
    require(!table.get(hash).is_valid() && table.get_pv_move(hash).is_null(),
            "a mismatched key/data pair must not reach the search");
    require(saved.data == a.data && saved.key == a.key,
            "a returned snapshot must remain independent of later writes");
}

void test_concurrent_tt_access(Searcher& searcher) {
    reset(searcher);
    auto& table = *searcher.trans_table;
    const u64 hash = 0x12345;
    std::array<TTEntry, 4> entries;
    for (size_t i = 0; i < entries.size(); ++i)
        entries[i] = TTEntry(hash, Move::normal(i, i + 8), 5,
                            -300 + static_cast<i32>(i) * 200, EXACT);
    table.put(entries[0], hash);

    std::atomic<unsigned> ready{0};
    std::atomic<bool> start{false};
    std::array<std::thread, 8> workers;
    for (size_t i = 0; i < workers.size(); ++i) {
        workers[i] = std::thread([&, i] {
            ready.fetch_add(1, std::memory_order_release);
            while (!start.load(std::memory_order_acquire))
                std::this_thread::yield();
            for (int iteration = 0; iteration < 25000; ++iteration) {
                if (i < entries.size()) table.put(entries[i], hash);
                const TTEntry result = table.get(hash);
                if (!result.is_valid()) continue; // Mixed snapshots may miss.
                require(std::any_of(entries.begin(), entries.end(),
                            [&](const TTEntry& entry) { return entry.data == result.data; }),
                        "a concurrent hit must contain one of the written payloads");
            }
        });
    }
    while (ready.load(std::memory_order_acquire) != workers.size())
        std::this_thread::yield();
    start.store(true, std::memory_order_release);
    const f32 ratio = table.valid_ratio();
    require(ratio >= 0 && ratio <= 1, "concurrent occupancy scans must stay in range");
    for (auto& worker : workers) worker.join();

    // Interleaved writers may leave a mismatched slot. A later generation must
    // still be able to replace it after all workers have stopped.
    ++table.age;
    table.put(entries[0], hash);
    const TTEntry result = table.get(hash);
    require(result.data == entries[0].data && result.age == table.age,
            "a quiescent write must restore a readable entry and generation");
}

void test_tt_check_extension_depth(Searcher& searcher) {
    Game game = Game::from_fen(
        "rnbqkbnr/p1p1pppp/1p1p4/8/Q1P1P3/8/PP1P1PPP/RNB1KBNR b KQkq - 2 3");
    const std::string before = fen(game);
    const u64 hash = game.hash;
    MoveList moves;
    require(gen_legal(game, moves).check != NO_CHECK,
            "the TT depth regression must exercise a check extension");

    reset(searcher);
    const i32 deep_score = searcher.alpha_beta(MIN_VALUE, MAX_VALUE, 2, 1, game, false);
    reset(searcher);
    const i32 shallow_score = searcher.alpha_beta(MIN_VALUE, MAX_VALUE, 1, 1, game, false);
    require(shallow_score != deep_score,
            "the TT depth regression needs different shallow and deep results");

    // Save actual depth-1 searches with each bound type. The check extension
    // must not let any of them satisfy a request for nominal depth 2.
    for (TTType type : {EXACT, UPPER, LOWER}) {
        reset(searcher);
        const i32 alpha = type == UPPER ? shallow_score + 1 : MIN_VALUE;
        const i32 beta = type == LOWER ? shallow_score - 1 : MAX_VALUE;
        searcher.alpha_beta(alpha, beta, 1, 1, game, false);
        const TTEntry shallow = searcher.trans_table->get(hash);
        require(shallow.is_valid() && shallow.get_type() == type,
                "the shallow search must save the intended TT bound");

        // Isolate this entry from all descendants and reset move ordering.
        reset(searcher);
        searcher.trans_table->put(shallow, hash);
        const i32 score = searcher.alpha_beta(alpha, beta, 2, 1, game, false);
        require(searcher.node_count > 1,
                "a check-extended depth-1 entry must not cut off depth 2");
        require(score == std::clamp(deep_score, alpha, beta),
                "a shallower cached check result must not replace the deeper result");
        require(fen(game) == before && game.hash == hash && game.state_stack.size == 0,
                "the TT depth regression must preserve the position");
    }
}

void test_quiescence_in_check(Searcher& searcher) {
    // The reported position after ...Qd6 Qa8+: Black has to play ...Kd7,
    // after which Rxf7 changes the evaluation. Standing pat skips that cost.
    Game game = Game::from_fen(
        "Q1kr4/1pp2p2/2bq2rp/3p4/2B5/2P3P1/P1P4P/1R3RK1 b - - 2 23");
    const std::string before = fen(game);
    const u64 hash = game.hash;
    MoveList moves;
    const GenResult generated = gen_non_quiet(game, moves);
    require(generated.check != NO_CHECK && generated.count == 1 &&
            moves[0].data == Move::normal(58, 51).data,
            "the quiescence regression needs the forced quiet evasion c8d7");

    reset(searcher);
    game.make_move(moves[0]);
    const i32 forced_score = -searcher.alpha_beta(MIN_VALUE, MAX_VALUE, 0, 3, game, false);
    game.unmake_move(moves[0]);
    require(forced_score < eval_game(game),
            "the forced evasion must score worse than standing pat");

    for (i32 beta : {MAX_VALUE, eval_game(game)}) {
        reset(searcher);
        const i32 score = searcher.alpha_beta(MIN_VALUE, beta, 0, 2, game, false);
        require(score == forced_score && searcher.node_count > 2,
                "quiescence in check must search the evasion without a stand-pat score or cutoff");
        require(fen(game) == before && game.hash == hash && game.state_stack.size == 0,
                "quiescence evasions must restore the position");
    }

    reset(searcher);
    game = Game::from_fen("7k/6Q1/6K1/8/8/8/8/8 b - - 0 1");
    const i32 score = searcher.alpha_beta(MIN_VALUE, eval_game(game) - 1, 0, 5, game, false);
    require(score == -MATE_VALUE + 5,
            "a stand-pat cutoff must never hide checkmate in quiescence");
}

void test_ply_limit(Searcher& searcher) {
    reset(searcher);
    for (const char* position : {
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
            "4k3/8/8/8/8/8/4r3/4K2R w - - 0 1"}) {
        Game game = Game::from_fen(position);
        const std::string before = fen(game);
        const u64 hash = game.hash;
        const i32 expected = eval_game(game);
        for (u8 ply : {u8{MAX_PLY}, std::numeric_limits<u8>::max()}) {
            for (u8 depth : {u8{0}, u8{2}}) {
                searcher.node_count = 0;
                const i32 score = searcher.alpha_beta(MIN_VALUE, MAX_VALUE,
                                                      depth, ply, game, false);
                require(score == expected && searcher.node_count == 1,
                        "search at or beyond MAX_PLY must stop without recursion");
            }
        }

        MoveList moves;
        const GenResult generated = gen_legal(game, moves);
        searcher.node_count = 0;
        searcher.alpha_beta(MIN_VALUE, MAX_VALUE, 2, MAX_PLY - 1, game, false);
        require(searcher.node_count > 1 && searcher.node_count <= generated.count + 1,
                "children must stop at MAX_PLY even after a check extension");
        require(fen(game) == before && game.hash == hash && game.state_stack.size == 0,
                "the ply limit must preserve the board, hash, and undo stack");
    }

    // Enter quiescence one ply below the boundary and exercise its capture path.
    Game game = Game::from_fen("4k3/8/8/3p4/3Q4/8/8/4K3 w - - 0 1");
    const std::string before = fen(game);
    const u64 hash = game.hash;
    MoveList moves;
    const GenResult generated = gen_non_quiet(game, moves);
    searcher.node_count = 0;
    searcher.alpha_beta(MIN_VALUE, MAX_VALUE, 0, MAX_PLY - 1, game, false);
    require(searcher.node_count > 2 && searcher.node_count <= generated.count + 2,
            "quiescence captures must also stop at MAX_PLY");
    require(fen(game) == before && game.hash == hash && game.state_stack.size == 0,
            "quiescence at the ply limit must restore the position");
}

void test_parallel_search(Searcher& searcher) {
    reset(searcher);
    searcher.set_search_time(200);
    Game game = Game::from_fen(
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    require(searcher.book.lookup_position(game).is_null(),
            "parallel search test must search outside the opening book");
    const std::string before = fen(game);
    const u64 hash = game.hash;
    MoveList moves;
    const GenResult generated = gen_legal(game, moves);
    for (int iteration = 0; iteration < 2; ++iteration) {
        const u16 age = searcher.trans_table->age;
        const Move result = searcher.get_best_move_parallel(game);
        require(std::any_of(std::begin(moves), std::begin(moves) + generated.count,
                            [&](Move move) { return move.data == result.data; }),
                "a nonzero-time parallel search must return a legal move");
        require(fen(game) == before && game.hash == hash && game.state_stack.size == 0,
                "parallel search must restore the caller's position");
        require(searcher.trans_table->age == static_cast<u16>(age + 1),
                "parallel search must advance the shared generation exactly once");
    }
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
    test_tt_snapshot_validation(searcher);
    test_concurrent_tt_access(searcher);
    test_tt_check_extension_depth(searcher);
    test_quiescence_in_check(searcher);
    test_ply_limit(searcher);
    test_parallel_search(searcher);
    test_saved_pv_first(searcher);
    test_root_bound_does_not_select_move(searcher);
    test_fail_low_preserves_move(searcher);
    test_interrupted_root_search(searcher, false);
    test_interrupted_root_search(searcher, true);
    test_interrupted_quiescence(searcher);
    test_interrupted_aspiration_retry(searcher);
    test_zero_time_and_terminal_positions(searcher);
    test_hash_resize(searcher);
    std::cout << "Search regression tests passed\n";
}
