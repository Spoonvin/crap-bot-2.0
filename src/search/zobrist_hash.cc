#include "search/zobrist_hash.h"
#include <iostream>

u64 sq_key_map[64][PIECE_COUNT*2 + 1];

u64 acting_player_key; // If white to play
u64 w_cstl_left_key;
u64 w_cstl_right_key;
u64 b_cstl_left_key;
u64 b_cstl_right_key;

void init_hash_key_map() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<u64> dist(0, std::numeric_limits<u64>::max() - 1);

    for (int square = 0; square < 64; square++) {
        for (int i = 0; i < PIECE_COUNT*2 + 1; i++) {
            sq_key_map[square][i] = dist(gen);
        }
    }
    acting_player_key = dist(gen); //If white use key
    w_cstl_right_key = dist(gen); //If true use keys
    w_cstl_left_key = dist(gen);
    b_cstl_right_key = dist(gen);
    b_cstl_left_key = dist(gen);
}

u64 gen_zob_hash(Game& game) {
    u64 hash = 0;

    for (Pos pos = 0; pos < 64; pos++) {
        Square sq = game.board[pos];
        if (sq == EMPTY_SQUARE) continue;

        Piece piece = stp(sq);
        Color c = stc(sq);
        u8 idx = (c == WHITE) ? piece : piece+PIECE_COUNT;
        hash ^= sq_key_map[pos][idx];
    }

    if (game.ep_pos >= 0) {
        hash ^= sq_key_map[game.ep_pos][EP_ZOB_INDEX];
    }

    if (!game.players[WHITE].king_moved()) {
        if (!game.players[WHITE].lrook_moved())
            hash ^= w_cstl_left_key;
        if (!game.players[WHITE].rrook_moved())
            hash ^= w_cstl_right_key;
    }

    if (!game.players[BLACK].king_moved()) {
        if (!game.players[BLACK].lrook_moved())
            hash ^= b_cstl_left_key;
        if (!game.players[BLACK].rrook_moved())
            hash ^= b_cstl_right_key;
    }

    if (game.turn == WHITE)
        hash ^= acting_player_key;

    return hash;
}