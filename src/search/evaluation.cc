#include "search/evaluation.h"
#include "chess/game.h"
#include "chess/board/mask_operations.h"

#include "chess/move/piece/king.h"
#include "chess/move/piece/knight.h"
#include "chess/move/piece/bishop.h"
#include "chess/move/piece/rook.h"
#include "chess/move/piece/queen.h"


#include <algorithm>

constexpr i32 DOUBLE_PAWN_PENALTY = 15;
constexpr i32 ISOLATED_PAWN_PENALTY = 15;

constexpr i32 QUEEN_OPEN_FILE_BONUS = 10;
constexpr i32 QUEEN_SEMI_OPEN_FILE_BONUS = 5;
constexpr i32 ROOK_OPEN_FILE_BONUS = 15;
constexpr i32 ROOK_SEMI_OPEN_FILE_BONUS = 8;

constexpr i32 BISHOP_PAIR_BONUS = 30;
constexpr i32 ROOK_ON_SEVENTH_BONUS = 20;

// Might need tuning
constexpr i32 KING_SHIELD_NEAR_BONUS = 10;
constexpr i32 KING_SHIELD_FAR_BONUS = 5;
constexpr i32 KING_SHIELD_MISSING_PENALTY = 10;
constexpr i32 KING_OPEN_FILE_PENALTY = 25;
constexpr i32 KING_SEMI_OPEN_FILE_PENALTY = 15;
constexpr i32 KING_ADJACENT_OPEN_FILE_PENALTY = 15;
constexpr i32 KING_ADJACENT_SEMI_OPEN_FILE_PENALTY = 10;


Mask WHITE_PASSED_TABLE[64];
Mask BLACK_PASSED_TABLE[64];
constexpr i32 PASSED_PAWN_BONUS[8] = {0, 5, 10, 20, 40, 70, 100, 0};

// How dangerous attacks from these pieces are to the king
constexpr i32 king_attack_weights[PIECE_COUNT] = {
    0, // Pawn
    3, // Rook
    2, // Knight
    2, // Bishop
    5, // Queen
    0, // King
};
// Multipliers for king attack scores depending on how many pieces attack. (percentage)
constexpr i32 coordination_mult[6] = {0, 25, 90, 110, 130, 150};

constexpr i32 safety_table[100] = {
    0,   0,   1,   2,   3,   5,   7,   9,   12,  15,
    18,  22,  26,  30,  35,  39,  44,  50,  56,  62,
    68,  75,  82,  85,  89,  97,  105, 113, 122, 131,
    140, 150, 169, 180, 191, 202, 213, 225, 237, 248,
    260, 272, 283, 295, 307, 319, 330, 342, 354, 366,
    377, 389, 401, 412, 424, 436, 448, 459, 471, 483,
    494, 500, 500, 500, 500, 500, 500, 500, 500, 500,
    500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
    500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
    500, 500, 500, 500, 500, 500, 500, 500, 500, 500
};

__attribute__((constructor))
void init_passed() {
    for (Pos pos = 0; pos < 64; pos++) {
        i8 row = pos / 8;
        i8 col = pos % 8;

        Mask mask = 0ULL;

        mask |= col_mask(col);
        if (col > 0)
            mask |= col_mask(col-1);
        if (col < 7)
            mask |= col_mask(col+1);

        Mask w_mask = mask;
        Mask b_mask = mask;

        // White: remove current row and all below
        for (i8 r = 0; r <= row; r++) {
            w_mask &= ~row_mask(r);
        }

        // Black: remove current row and all above
        for (i8 r = 7; r >= row; r--) {
            b_mask &= ~row_mask(r);
        }

        WHITE_PASSED_TABLE[pos] = w_mask;
        BLACK_PASSED_TABLE[pos] = b_mask;
    }
}

const i8 b_pawn_square_mod[64] = {
    0,  0,  0,  0,  0,  0,  0,  0,
    20, 20, 30, 30, 30, 30, 20, 10,
    10, 10, 15, 15, 15, 15, 10, 10,
    5,  5,  10, 10, 10, 10,  5,  0,
    0,  0,  0,  20, 20,  0,  0,  0,
    5, -5,  0,  0,  0,  0, 10,  5,
    5,  0, 10,-20,-20, 10, 10,  0,
    0,  0,  0,  0,  0,  0,  0,  0};

const i8 b_knight_square_mod[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50};

const i8 b_bishop_square_mod[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -20,-10,-10,-10,-10,-10,-10,-20};

const i8 b_rook_square_mod[64] = {
    0,  0,  0,  0,  0,  0,  0,  0,
    5, 10, 10, 10, 10, 10, 10,  5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    0,  0,  0,  5,  5,  0,  0,  0};

const i8 b_queen_square_mod[64] = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,
    -5,  0,  5,  5,  5,  5,  0, -5,
     0,  0,  5,  5,  5,  5,  0, -5,
    -10,  5,  5,  5,  5,  5,  0,-10,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20};

const i8 b_king_square_mod[64] = {
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -10,-20,-20,-20,-20,-20,-20,-10,
    5 ,  5,  0,-25,-25,  0,  5,  5,
    0, 30, 10, -15, 0, -15, 30, 20};

const i8 b_king_square_mod_end[64] = {	
	-50, -10, 0,  0	, 0	, 0,  -10, -50	,
	-10, 0	, 10, 10, 10, 10, 0,   -10	,
	0,	 10	, 20, 20, 20, 20, 10,   0	,
	0,	 10	, 20, 40, 40, 20, 10,	0	,
	0,	 10	, 20, 40, 40, 20, 10,	0	,
	0,	 10	, 20, 20, 20, 20, 10,	0	,
	-10, 0	, 10, 10, 10, 10, 0,    -10	,
	-50, -10, 0	, 0	, 0	, 0,  -10,	-50	
};

Pos invert_pos(Pos pos) {
    i8 new_pos = ((pos + 56)) - ((pos / 8) * 16);
    return new_pos;
}

i32 eval_game_old(Game& game) {
    const f32 endgame = endgame_ratio(game);
    Bitboard bitboard_white = game.players[WHITE].bb;
    Bitboard bitboard_black = game.players[BLACK].bb;

    Mask all_pawns = bitboard_white[PAWN] | bitboard_black[PAWN];

    i32 white_score = 0;
    i32 black_score = 0;

    // Pawn structure
    white_score += eval_pawn_structure(bitboard_white[PAWN], bitboard_black[PAWN], WHITE);
    black_score += eval_pawn_structure(bitboard_black[PAWN], bitboard_white[PAWN], BLACK);

    // King safety evaluation
    // Pawn shelter should not discourage king activity in pawn endings.
    white_score += static_cast<i32>((1.0f - endgame) * eval_pawn_shield(
        game.players, __builtin_ctzll(bitboard_white[KING]), WHITE));
    black_score += static_cast<i32>((1.0f - endgame) * eval_pawn_shield(
        game.players, __builtin_ctzll(bitboard_black[KING]), BLACK));

    // ------------ White ------------

    // White Knights
    while (bitboard_white[KNIGHT]) {
        Pos pos = invert_pos(pop_pos(bitboard_white[KNIGHT]));
        white_score += KNIGHT_VALUE + b_knight_square_mod[pos];
    }

    // White Bishops
    u8 num_w_bishops = 0;
    while (bitboard_white[BISHOP]) {
        Pos pos = invert_pos(pop_pos(bitboard_white[BISHOP]));
        white_score += BISHOP_VALUE + b_bishop_square_mod[pos];
        num_w_bishops++;
    }
    if (num_w_bishops >= 2) {
        white_score += BISHOP_PAIR_BONUS;
    }

    // White Rooks
    while (bitboard_white[ROOK]) {
        Pos pos = invert_pos(pop_pos(bitboard_white[ROOK]));
        white_score += ROOK_VALUE + b_rook_square_mod[pos];

        if (!(all_pawns & col_mask(pos % 8))) {
            white_score += ROOK_OPEN_FILE_BONUS;
        } else if (!(bitboard_white[PAWN] & col_mask(pos % 8))) {
            white_score += ROOK_SEMI_OPEN_FILE_BONUS;
        }
    }

    // White Queens
    while (bitboard_white[QUEEN]) {
        Pos pos = invert_pos(pop_pos(bitboard_white[QUEEN]));
        white_score += QUEEN_VALUE + b_queen_square_mod[pos];

        if (!(all_pawns & col_mask(pos % 8))) {
            white_score += QUEEN_OPEN_FILE_BONUS;
        } else if (!(bitboard_white[PAWN] & col_mask(pos % 8))) {
            white_score += QUEEN_SEMI_OPEN_FILE_BONUS;
        }
    }

    // White Pawns
    while (bitboard_white[PAWN]) {
        Pos pos = invert_pos(pop_pos(bitboard_white[PAWN]));
        white_score += PAWN_VALUE + b_pawn_square_mod[pos];
    }

    Pos king_pos_w = invert_pos(__builtin_ctzll(bitboard_white[KING]));
    white_score += static_cast<i32>(
        (1.0f - endgame) * b_king_square_mod[king_pos_w] +
        endgame * b_king_square_mod_end[king_pos_w]);

    // ------------ Black -----------------------

    // Black Knights
    while (bitboard_black[KNIGHT]) {
        Pos pos = pop_pos(bitboard_black[KNIGHT]);
        black_score += KNIGHT_VALUE + b_knight_square_mod[pos];
    }

    // Black Bishops
    u8 num_b_bishops = 0;
    while (bitboard_black[BISHOP]) {
        Pos pos = pop_pos(bitboard_black[BISHOP]);
        black_score += BISHOP_VALUE + b_bishop_square_mod[pos];
        num_b_bishops++;
    }
    if (num_b_bishops >= 2)
        black_score += BISHOP_PAIR_BONUS;

    // Black Rooks
    while (bitboard_black[ROOK]) {
        Pos pos = pop_pos(bitboard_black[ROOK]);
        black_score += ROOK_VALUE + b_rook_square_mod[pos];

        if (!(all_pawns & col_mask(pos % 8))) {
            black_score += ROOK_OPEN_FILE_BONUS;
        } else if (!(bitboard_black[PAWN] & col_mask(pos % 8))) {
            black_score += ROOK_SEMI_OPEN_FILE_BONUS;
        }
    }

    // Black Queens
    while (bitboard_black[QUEEN]) {
        Pos pos = pop_pos(bitboard_black[QUEEN]);
        black_score += QUEEN_VALUE + b_queen_square_mod[pos];

        if (!(all_pawns & col_mask(pos % 8))) {
            black_score += QUEEN_OPEN_FILE_BONUS;
        } else if (!(bitboard_black[PAWN] & col_mask(pos % 8))) {
            black_score += QUEEN_SEMI_OPEN_FILE_BONUS;
        }
    }

    // Black Pawns
    while (bitboard_black[PAWN]) {
        Pos pos = pop_pos(bitboard_black[PAWN]);
        black_score += PAWN_VALUE + b_pawn_square_mod[pos];
    }

    Pos king_pos_b = __builtin_ctzll(bitboard_black[KING]);
    black_score += static_cast<i32>(
        (1.0f - endgame) * b_king_square_mod[king_pos_b] +
        endgame * b_king_square_mod_end[king_pos_b]);

    i32 result = (game.turn == WHITE) ? white_score - black_score : black_score - white_score;

    return result;
}


template<Piece P>
inline void add_king_pressure(Pos s, Mask occupancy, Mask zone, int& count, int& weight) {
    Mask attack;
    if constexpr (P == KNIGHT) attack = knight_atk_mask(s);
    if constexpr (P == BISHOP) attack = bishop_atk_mask(occupancy, s);
    if constexpr (P == ROOK) attack = rook_atk_mask(occupancy, s);
    if constexpr (P == QUEEN) attack = queen_atk_mask(occupancy, s);
    Mask hits = attack & zone;
    if (hits) {
        ++count;
        weight += king_attack_weights[P] * __builtin_popcountll(hits);
    }
}

template<Color C>
inline Mask calc_king_zone(Pos king_pos) {
    Mask zone = king_atk_mask(king_pos) | pos_mask(king_pos);
    if constexpr (C == WHITE) {
        if (king_pos + 8 < 64)
            zone |= king_atk_mask(king_pos + 8);
    } else {
        if (king_pos - 8 >= 0)
            zone |= king_atk_mask(king_pos - 8);
    }
    return zone;
}

i32 eval_game(Game& game) {
    const f32 endgame = endgame_ratio(game);
    Bitboard bitboard_white = game.players[WHITE].bb;
    Bitboard bitboard_black = game.players[BLACK].bb;

    Mask all_pawns = bitboard_white[PAWN] | bitboard_black[PAWN];

    i32 white_score = 0;
    i32 black_score = 0;

    // Pawn structure
    white_score += eval_pawn_structure(bitboard_white[PAWN], bitboard_black[PAWN], WHITE);
    black_score += eval_pawn_structure(bitboard_black[PAWN], bitboard_white[PAWN], BLACK);

    // King positions/mask
    const Pos kings[2] = {Pos(__builtin_ctzll(bitboard_white[KING])), Pos(__builtin_ctzll(bitboard_black[KING]))};
    const Mask zones[2] = {calc_king_zone<WHITE>(kings[0]), calc_king_zone<BLACK>(kings[1])};

    const Mask occupancy = bitboard_white.occupancy() | bitboard_black.occupancy();

    // Shield bonus/penalty and attacker counts + weights
    const i32 shields[2] = {eval_pawn_shield(game.players, kings[0], WHITE), eval_pawn_shield(game.players, kings[1], BLACK)};
    i32 counts[2] = {};
    i32 weights[2] = {};

    // ------------ White ------------

    // White Knights
    while (bitboard_white[KNIGHT]) {
        Pos board_pos = pop_pos(bitboard_white[KNIGHT]);
        add_king_pressure<KNIGHT>(board_pos, occupancy, zones[BLACK], counts[BLACK], weights[BLACK]);
        Pos pos = invert_pos(board_pos);
        white_score += KNIGHT_VALUE + b_knight_square_mod[pos];
    }

    // White Bishops
    u8 num_w_bishops = 0;
    while (bitboard_white[BISHOP]) {
        Pos board_pos = pop_pos(bitboard_white[BISHOP]);
        add_king_pressure<BISHOP>(board_pos, occupancy, zones[BLACK], counts[BLACK], weights[BLACK]);
        Pos pos = invert_pos(board_pos);
        white_score += BISHOP_VALUE + b_bishop_square_mod[pos];
        num_w_bishops++;
    }
    if (num_w_bishops >= 2) {
        white_score += BISHOP_PAIR_BONUS;
    }

    // White Rooks
    while (bitboard_white[ROOK]) {
        Pos board_pos = pop_pos(bitboard_white[ROOK]);
        add_king_pressure<ROOK>(board_pos, occupancy, zones[BLACK], counts[BLACK], weights[BLACK]);
        Pos pos = invert_pos(board_pos);
        white_score += ROOK_VALUE + b_rook_square_mod[pos];

        if (!(all_pawns & col_mask(pos % 8))) {
            white_score += ROOK_OPEN_FILE_BONUS;
        } else if (!(bitboard_white[PAWN] & col_mask(pos % 8))) {
            white_score += ROOK_SEMI_OPEN_FILE_BONUS;
        }
    }

    // White Queens
    while (bitboard_white[QUEEN]) {
        Pos board_pos = pop_pos(bitboard_white[QUEEN]);
        add_king_pressure<QUEEN>(board_pos, occupancy, zones[BLACK], counts[BLACK], weights[BLACK]);
        Pos pos = invert_pos(board_pos);
        white_score += QUEEN_VALUE + b_queen_square_mod[pos];

        if (!(all_pawns & col_mask(pos % 8))) {
            white_score += QUEEN_OPEN_FILE_BONUS;
        } else if (!(bitboard_white[PAWN] & col_mask(pos % 8))) {
            white_score += QUEEN_SEMI_OPEN_FILE_BONUS;
        }
    }

    // White Pawns
    while (bitboard_white[PAWN]) {
        Pos pos = invert_pos(pop_pos(bitboard_white[PAWN]));
        white_score += PAWN_VALUE + b_pawn_square_mod[pos];
    }

    Pos king_pos_w = invert_pos(__builtin_ctzll(bitboard_white[KING]));
    white_score += static_cast<i32>(
        (1.0f - endgame) * b_king_square_mod[king_pos_w] +
        endgame * b_king_square_mod_end[king_pos_w]);

    // ------------ Black -----------------------

    // Black Knights
    while (bitboard_black[KNIGHT]) {
        Pos board_pos = pop_pos(bitboard_black[KNIGHT]);
        add_king_pressure<KNIGHT>(board_pos, occupancy, zones[WHITE], counts[WHITE], weights[WHITE]);
        Pos pos = board_pos;
        black_score += KNIGHT_VALUE + b_knight_square_mod[pos];
    }

    // Black Bishops
    u8 num_b_bishops = 0;
    while (bitboard_black[BISHOP]) {
        Pos board_pos = pop_pos(bitboard_black[BISHOP]);
        add_king_pressure<BISHOP>(board_pos, occupancy, zones[WHITE], counts[WHITE], weights[WHITE]);
        Pos pos = board_pos;
        black_score += BISHOP_VALUE + b_bishop_square_mod[pos];
        num_b_bishops++;
    }
    if (num_b_bishops >= 2)
        black_score += BISHOP_PAIR_BONUS;

    // Black Rooks
    while (bitboard_black[ROOK]) {
        Pos board_pos = pop_pos(bitboard_black[ROOK]);
        add_king_pressure<ROOK>(board_pos, occupancy, zones[WHITE], counts[WHITE], weights[WHITE]);
        Pos pos = board_pos;
        black_score += ROOK_VALUE + b_rook_square_mod[pos];

        if (!(all_pawns & col_mask(pos % 8))) {
            black_score += ROOK_OPEN_FILE_BONUS;
        } else if (!(bitboard_black[PAWN] & col_mask(pos % 8))) {
            black_score += ROOK_SEMI_OPEN_FILE_BONUS;
        }
    }

    // Black Queens
    while (bitboard_black[QUEEN]) {
        Pos board_pos = pop_pos(bitboard_black[QUEEN]);
        add_king_pressure<QUEEN>(board_pos, occupancy, zones[WHITE], counts[WHITE], weights[WHITE]);
        Pos pos = board_pos;
        black_score += QUEEN_VALUE + b_queen_square_mod[pos];

        if (!(all_pawns & col_mask(pos % 8))) {
            black_score += QUEEN_OPEN_FILE_BONUS;
        } else if (!(bitboard_black[PAWN] & col_mask(pos % 8))) {
            black_score += QUEEN_SEMI_OPEN_FILE_BONUS;
        }
    }

    // Black Pawns
    while (bitboard_black[PAWN]) {
        Pos pos = pop_pos(bitboard_black[PAWN]);
        black_score += PAWN_VALUE + b_pawn_square_mod[pos];
    }

    Pos king_pos_b = __builtin_ctzll(bitboard_black[KING]);
    black_score += static_cast<i32>(
        (1.0f - endgame) * b_king_square_mod[king_pos_b] +
        endgame * b_king_square_mod_end[king_pos_b]);

    const i32 wp = weights[WHITE] * coordination_mult[std::min(counts[WHITE], 5)] / 100;
    const i32 bp = weights[BLACK] * coordination_mult[std::min(counts[BLACK], 5)] / 100;
    white_score += static_cast<i32>((1.0f - endgame) * (shields[WHITE] - safety_table[std::min(wp, 99)]));
    black_score += static_cast<i32>((1.0f - endgame) * (shields[BLACK] - safety_table[std::min(bp, 99)]));

    const i32 result = (game.turn == WHITE) ? white_score - black_score : black_score - white_score;

    return result;
}

f32 endgame_ratio(const Game& game) {
    // Knights/bishops: 1, rooks: 2, queens: 4. Total 24.
    constexpr i32 starting_phase = 24;
    i32 phase = 0;
    for (int color = 0; color < 2; ++color) {
        const auto& pieces = game.players[color].bb.masks;
        phase += __builtin_popcountll(pieces[KNIGHT] | pieces[BISHOP]);
        phase += 2 * __builtin_popcountll(pieces[ROOK]);
        phase += 4 * __builtin_popcountll(pieces[QUEEN]);
    }

    return static_cast<f32>(starting_phase - std::min(phase, starting_phase)) /
           starting_phase;
}

i32 square_value(Square square) {
    if (square == EMPTY_SQUARE) return 0;

    const Piece piece = stp(square);

    switch(piece) {
        case PAWN:
            return PAWN_VALUE;
            break;
        case KNIGHT:
            return KNIGHT_VALUE;
            break;
        case BISHOP:
            return BISHOP_VALUE;
            break;
        case ROOK:
            return ROOK_VALUE;
            break;
        case QUEEN:
            return QUEEN_VALUE;
            break;
        case KING:
            return KING_VALUE;
            break;
    }

    return 0;
}

bool is_quiet(Move move, Game& game) {
    return move.type() != PROMOTION
        && move.type() != EN_PASSANT
        && game.board[move.to()] == EMPTY_SQUARE;
}

i32 calc_move_score(Move move, Game& game) {
    const Pos from = move.from();
    const Pos to = move.to();
    const Square attacker = game.board[from];
    const Square victim = game.board[to];
    i32 a_val = square_value(attacker);
    i32 v_val = square_value(victim);

    i32 promo_bonus = 0;
    if (move.type() == PROMOTION) {
        switch (move.promo_piece()) {
            case KNIGHT:
            case BISHOP:
                promo_bonus = 300;
                break;
            case ROOK:
                promo_bonus = 500;
                break;
            case QUEEN:
                promo_bonus = 900;
                break;
            default:
                break;
        }
    } else if (move.type() == EN_PASSANT) {
        v_val = PAWN_VALUE;
    }

    if (v_val == 0)
        a_val = 0;

    return v_val - (a_val / 100) + promo_bonus;
}

i32 calc_move_score_old(Move move, Game& game) {
    const Pos from = move.from();
    const Pos to = move.to();
    const Square attacker = game.board[from];
    const Square victim = game.board[to];
    i32 a_val = square_value(attacker);
    i32 v_val = square_value(victim);

    return v_val - a_val;
}

i32 eval_pawn_structure(Mask acting_pawns, Mask opponent_pawns, Color acting_color) {

    i32 total_score = 0;

    for (int col = 0; col < 8; col++) {
        Mask file = col_mask(col);
        Mask pawns_on_file = acting_pawns & file;

        if (!pawns_on_file) continue;

        int double_count = __builtin_popcountll(pawns_on_file);

        if (double_count > 1) {
            total_score -= (double_count - 1) * DOUBLE_PAWN_PENALTY;
        }

        Mask adjacent = 0;
        if (col > 0) adjacent |= col_mask(col - 1);
        if (col < 7) adjacent |= col_mask(col + 1);

        if ((acting_pawns & adjacent) == 0) {
            int iso_count = __builtin_popcountll(pawns_on_file);
            total_score -= iso_count * ISOLATED_PAWN_PENALTY;
        }
    }

    Mask temp = acting_pawns;

    while (temp) {
        Pos pos = pop_pos(temp);
        Mask file = col_mask(pos % 8);
        Mask pawns_on_file = acting_pawns & file;

        // Find pawn closest to promotion
        pos = (acting_color == WHITE)
            ? 63 - __builtin_clzll(pawns_on_file)
            : __builtin_ctzll(pawns_on_file);

        temp &= ~file;

        Mask passed_mask = (acting_color == WHITE)
            ? WHITE_PASSED_TABLE[pos]
            : BLACK_PASSED_TABLE[pos];

        if ((opponent_pawns & passed_mask) == 0) {
            i8 row = pos / 8;

            // Flip for black so both use 0-7 progression
            if (acting_color == BLACK) row = 7 - row;

            total_score += PASSED_PAWN_BONUS[row];
        }
    }

    return total_score;
}

i32 eval_king_safety_old(const Player players[2], Pos king_pos, Color acting_color) {
    const i32 direction = (acting_color == WHITE) ? 8 : -8;
    const i32 king_col = king_pos % 8;
    const i32 front_row_start = king_pos - king_col + direction;

    const Mask own_pawns = players[acting_color].bb.masks[PAWN];
    const Mask enemy_pawns = players[!acting_color].bb.masks[PAWN];
    const Mask forward = (acting_color == WHITE)
        ? WHITE_PASSED_TABLE[king_pos] : BLACK_PASSED_TABLE[king_pos];

    i32 score = 0;
    const i32 start = (king_col <= 0) ? 0 : king_col - 1;
    const i32 end = (king_col >= 7) ? 7 : king_col + 1;
    for (i32 col = start; col <= end; ++col) {
        const Mask file = col_mask(col);
        const i32 front_pos = front_row_start + col;
        const i32 farther_pos = front_pos + direction;

        // Only one bonus possible per file
        if (front_pos >= 0 && front_pos < 64 && (own_pawns & pos_mask(front_pos)))
            score += KING_SHIELD_NEAR_BONUS;
        else if (farther_pos >= 0 && farther_pos < 64 && (own_pawns & pos_mask(farther_pos)))
            score += KING_SHIELD_FAR_BONUS;
        else
            score -= KING_SHIELD_MISSING_PENALTY;

        // Give open and semi-open file penalties
        // Extra penalty if king is on open file
        const Mask approach = forward & file;
        if (!(own_pawns & approach)) {
            const bool king_file = col == king_col;
            if (!(enemy_pawns & approach))
                score -= king_file ? KING_OPEN_FILE_PENALTY
                                   : KING_ADJACENT_OPEN_FILE_PENALTY;
            else
                score -= king_file ? KING_SEMI_OPEN_FILE_PENALTY
                                   : KING_ADJACENT_SEMI_OPEN_FILE_PENALTY;
        }
    }

    // Future improvement, extend king zone
    Mask king_zone = king_atk_mask(king_pos) | pos_mask(king_pos);
    const Mask occupancy = players[WHITE].bb.occupancy() | players[BLACK].bb.occupancy();

    i32 num_attackers = 0;
    i32 weighted_attack_score = 0;

    // Loops over non-pawn pieces, excluding pawns
    for (i8 i = 1; i < PIECE_COUNT-1; i++) {
        Piece piece = Piece(i);
        Mask attacker = players[!acting_color].bb.masks[piece];
        Mask attack = 0;

        while (attacker) {
            Pos a_pos = pop_pos(attacker);
            switch (piece) {
                case KNIGHT:
                    attack = knight_atk_mask(a_pos);
                    break;
                case BISHOP:
                    attack = bishop_atk_mask(occupancy, a_pos);
                    break;
                case ROOK:
                    attack = rook_atk_mask(occupancy, a_pos);
                    break;
                case QUEEN:
                    attack = queen_atk_mask(occupancy, a_pos);
                    break;
                default:
                    break;
            }

            i32 num_attacks = __builtin_popcountll(attack & king_zone);

            if (num_attacks > 0) {
                num_attackers++;
                weighted_attack_score += king_attack_weights[piece]*num_attacks;
            }
        }
    }

    i32 attack_pressure = 
        weighted_attack_score * coordination_mult[std::min(num_attackers, 5)] / 100;

    // Vibe formula, seems to match CPWs table somewhat
    score -= attack_pressure*attack_pressure / 4; //safety_table[std::min(attack_pressure, 99)];

    return score;
}

i32 eval_pawn_shield(const Player players[2], Pos king_pos, Color acting_color) {
    const i32 direction = (acting_color == WHITE) ? 8 : -8;
    const i32 king_col = king_pos % 8;
    const i32 front_row_start = king_pos - king_col + direction;

    const Mask own_pawns = players[acting_color].bb.masks[PAWN];
    const Mask enemy_pawns = players[!acting_color].bb.masks[PAWN];
    const Mask forward = (acting_color == WHITE)
        ? WHITE_PASSED_TABLE[king_pos] : BLACK_PASSED_TABLE[king_pos];

    i32 score = 0;
    const i32 start = (king_col <= 0) ? 0 : king_col - 1;
    const i32 end = (king_col >= 7) ? 7 : king_col + 1;
    for (i32 col = start; col <= end; ++col) {
        const Mask file = col_mask(col);
        const i32 front_pos = front_row_start + col;
        const i32 farther_pos = front_pos + direction;

        // Only one bonus possible per file
        if (front_pos >= 0 && front_pos < 64 && (own_pawns & pos_mask(front_pos)))
            score += KING_SHIELD_NEAR_BONUS;
        else if (farther_pos >= 0 && farther_pos < 64 && (own_pawns & pos_mask(farther_pos)))
            score += KING_SHIELD_FAR_BONUS;
        else
            score -= KING_SHIELD_MISSING_PENALTY;

        // Give open and semi-open file penalties
        // Extra penalty if king is on open file
        const Mask approach = forward & file;
        if (!(own_pawns & approach)) {
            const bool king_file = col == king_col;
            if (!(enemy_pawns & approach))
                score -= king_file ? KING_OPEN_FILE_PENALTY
                                   : KING_ADJACENT_OPEN_FILE_PENALTY;
            else
                score -= king_file ? KING_SEMI_OPEN_FILE_PENALTY
                                   : KING_ADJACENT_SEMI_OPEN_FILE_PENALTY;
        }
    }

    return score;
}