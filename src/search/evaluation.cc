#include "search/evaluation.h"
#include "chess/game.h"
#include "chess/board/mask_operations.h"

#include "chess/move/piece/king.h"
#include "chess/move/piece/knight.h"
#include "chess/move/piece/bishop.h"
#include "chess/move/piece/rook.h"
#include "chess/move/piece/queen.h"
#include "chess/move/piece/white_pawn.h"
#include "chess/move/piece/black_pawn.h"


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
    4, // Queen
    0, // King
};
// Multipliers for king attack scores depending on how many pieces attack. (percentage)
constexpr i32 coordination_mult[6] = {0, 25, 90, 100, 130, 150};

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

// Stockfish mobility values, indexed by the number of attacked squares.
// Each entry is indexed {middlegame, endgame}.
constexpr i32 KNIGHT_MOBILITY_BONUS[9][2] = {
    {-62, -81}, {-53, -56}, {-12, -30}, { -4, -14}, {  3,   8},
    { 13,  15}, { 22,  23}, { 28,  27}, { 33,  33}
};

constexpr i32 BISHOP_MOBILITY_BONUS[14][2] = {
    {-48, -59}, {-20, -23}, { 16,  -3}, { 26,  13}, { 38,  24},
    { 51,  42}, { 55,  54}, { 63,  57}, { 63,  65}, { 68,  73},
    { 81,  78}, { 81,  86}, { 91,  88}, { 98,  97}
};

constexpr i32 ROOK_MOBILITY_BONUS[15][2] = {
    {-58, -76}, {-27, -18}, {-15,  28}, {-10,  55}, { -5,  69},
    { -2,  82}, {  9, 112}, { 16, 118}, { 30, 132}, { 29, 142},
    { 32, 155}, { 38, 165}, { 46, 166}, { 48, 169}, { 58, 171}
};

constexpr i32 QUEEN_MOBILITY_BONUS[28][2] = {
    {-39, -36}, {-21, -15}, {  3,   8}, {  3,  18}, { 14,  34},
    { 22,  54}, { 28,  61}, { 41,  73}, { 43,  79}, { 48,  92},
    { 56,  94}, { 60, 104}, { 60, 113}, { 66, 120}, { 67, 123},
    { 70, 126}, { 71, 133}, { 73, 136}, { 79, 140}, { 88, 143},
    { 88, 148}, { 99, 166}, {102, 170}, {102, 175}, {106, 184},
    {109, 191}, {113, 206}, {116, 212}
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

// -------- Pesto tables -----------

constexpr i32 mg_pawn_table[64] = {
      0,   0,   0,   0,   0,   0,  0,   0,
     98, 134,  61,  95,  68, 126, 34, -11,
     -6,   7,  26,  31,  65,  56, 25, -20,
    -14,  13,   6,  21,  23,  12, 17, -23,
    -27,  -2,  -5,  12,  17,   6, 10, -25,
    -26,  -4,  -4, -10,   3,   3, 33, -12,
    -35,  -1, -20, -23, -15,  24, 38, -22,
      0,   0,   0,   0,   0,   0,  0,   0,
};

constexpr i32 eg_pawn_table[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
    178, 173, 158, 134, 147, 132, 165, 187,
     94, 100,  85,  67,  56,  53,  82,  84,
     32,  24,  13,   5,  -2,   4,  17,  17,
     13,   9,  -3,  -7,  -7,  -8,   3,  -1,
      4,   7,  -6,   1,   0,  -5,  -1,  -8,
     13,   8,   8,  10,  13,   0,   2,  -7,
      0,   0,   0,   0,   0,   0,   0,   0,
};

constexpr i32 mg_knight_table[64] = {
    -167, -89, -34, -49,  61, -97, -15, -107,
     -73, -41,  72,  36,  23,  62,   7,  -17,
     -47,  60,  37,  65,  84, 129,  73,   44,
      -9,  17,  19,  53,  37,  69,  18,   22,
     -13,   4,  16,  13,  28,  19,  21,   -8,
     -23,  -9,  12,  10,  19,  17,  25,  -16,
     -29, -53, -12,  -3,  -1,  18, -14,  -19,
    -105, -21, -58, -33, -17, -28, -19,  -23,
};

constexpr i32 eg_knight_table[64] = {
    -58, -38, -13, -28, -31, -27, -63, -99,
    -25,  -8, -25,  -2,  -9, -25, -24, -52,
    -24, -20,  10,   9,  -1,  -9, -19, -41,
    -17,   3,  22,  22,  22,  11,   8, -18,
    -18,  -6,  16,  25,  16,  17,   4, -18,
    -23,  -3,  -1,  15,  10,  -3, -20, -22,
    -42, -20, -10,  -5,  -2, -20, -23, -44,
    -29, -51, -23, -15, -22, -18, -50, -64,
};

constexpr i32 mg_bishop_table[64] = {
    -29,   4, -82, -37, -25, -42,   7,  -8,
    -26,  16, -18, -13,  30,  59,  18, -47,
    -16,  37,  43,  40,  35,  50,  37,  -2,
     -4,   5,  19,  50,  37,  37,   7,  -2,
     -6,  13,  13,  26,  34,  12,  10,   4,
      0,  15,  15,  15,  14,  27,  18,  10,
      4,  15,  16,   0,   7,  21,  33,   1,
    -33,  -3, -14, -21, -13, -12, -39, -21,
};

constexpr i32 eg_bishop_table[64] = {
    -14, -21, -11,  -8, -7,  -9, -17, -24,
     -8,  -4,   7, -12, -3, -13,  -4, -14,
      2,  -8,   0,  -1, -2,   6,   0,   4,
     -3,   9,  12,   9, 14,  10,   3,   2,
     -6,   3,  13,  19,  7,  10,  -3,  -9,
    -12,  -3,   8,  10, 13,   3,  -7, -15,
    -14, -18,  -7,  -1,  4,  -9, -15, -27,
    -23,  -9, -23,  -5, -9, -16,  -5, -17,
};

constexpr i32 mg_rook_table[64] = {
     32,  42,  32,  51, 63,  9,  31,  43,
     27,  32,  58,  62, 80, 67,  26,  44,
     -5,  19,  26,  36, 17, 45,  61,  16,
    -24, -11,   7,  26, 24, 35,  -8, -20,
    -36, -26, -12,  -1,  9, -7,   6, -23,
    -45, -25, -16, -17,  3,  0,  -5, -33,
    -44, -16, -20,  -9, -1, 11,  -6, -71,
    -19, -13,   1,  17, 16,  7, -37, -26,
};

constexpr i32 eg_rook_table[64] = {
    13, 10, 18, 15, 12,  12,   8,   5,
    11, 13, 13, 11, -3,   3,   8,   3,
     7,  7,  7,  5,  4,  -3,  -5,  -3,
     4,  3, 13,  1,  2,   1,  -1,   2,
     3,  5,  8,  4, -5,  -6,  -8, -11,
    -4,  0, -5, -1, -7, -12,  -8, -16,
    -6, -6,  0,  2, -9,  -9, -11,  -3,
    -9,  2,  3, -1, -5, -13,   4, -20,
};

constexpr i32 mg_queen_table[64] = {
    -28,   0,  29,  12,  59,  44,  43,  45,
    -24, -39,  -5,   1, -16,  57,  28,  54,
    -13, -17,   7,   8,  29,  56,  47,  57,
    -27, -27, -16, -16,  -1,  17,  -2,   1,
     -9, -26,  -9, -10,  -2,  -4,   3,  -3,
    -14,   2, -11,  -2,  -5,   2,  14,   5,
    -35,  -8,  11,   2,   8,  15,  -3,   1,
     -1, -18,  -9,  10, -15, -25, -31, -50,
};

constexpr i32 eg_queen_table[64] = {
     -9,  22,  22,  27,  27,  19,  10,  20,
    -17,  20,  32,  41,  58,  25,  30,   0,
    -20,   6,   9,  49,  47,  35,  19,   9,
      3,  22,  24,  45,  57,  40,  57,  36,
    -18,  28,  19,  47,  31,  34,  39,  23,
    -16, -27,  15,   6,   9,  17,  10,   5,
    -22, -23, -30, -16, -16, -23, -36, -32,
    -33, -28, -22, -43,  -5, -32, -20, -41,
};

constexpr i32 mg_king_table[64] = {
    -65,  23,  16, -15, -56, -34,   2,  13,
     29,  -1, -20,  -7,  -8,  -4, -38, -29,
     -9,  24,   2, -16, -20,   6,  22, -22,
    -17, -20, -12, -27, -30, -25, -14, -36,
    -49,  -1, -27, -39, -46, -44, -33, -51,
    -14, -14, -22, -46, -44, -30, -15, -27,
      1,   7,  -8, -64, -43, -16,   9,   8,
    -15,  36,  12, -54,   8, -28,  24,  14,
};

constexpr i32 eg_king_table[64] = {
    -74, -35, -18, -18, -11,  15,   4, -17,
    -12,  17,  14,  17,  17,  38,  23,  11,
     10,  17,  23,  15,  20,  45,  44,  13,
     -8,  22,  24,  27,  26,  33,  26,   3,
    -18,  -4,  21,  24,  27,  23,   9, -11,
    -19,  -3,  11,  21,  23,  16,   7,  -9,
    -27, -11,   4,  13,  14,   4,  -5, -17,
    -53, -34, -21, -11, -28, -14, -24, -43
};

// PAWN, ROOK, KNIGHT, BISHOP, QUEEN, KING
constexpr i32 mg_piece_value[6] = { 82, 477, 337, 365, 1025,  0};
constexpr i32 eg_piece_value[6] = { 94, 512, 281, 297, 936,  0};

// ---------- End of pesto -----------------

Pos invert_pos(Pos pos) {
    i8 new_pos = ((pos + 56)) - ((pos / 8) * 16);
    return new_pos;
}

template<Piece P>
inline void add_king_pressure(Mask zone, Mask attack, int& count, int& weight) {
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

i32 eval_game_old(Game& game) {
    const f32 endgame = endgame_ratio(game);

    Bitboard bitboard_white = game.players[WHITE].bb;
    Bitboard bitboard_black = game.players[BLACK].bb;

    const Mask w_rooks = bitboard_white[ROOK];
    const Mask b_rooks = bitboard_black[ROOK];
    const Mask w_queens = bitboard_white[QUEEN];
    const Mask b_queens = bitboard_black[QUEEN];

    const Mask all_pawns = bitboard_white[PAWN] | bitboard_black[PAWN];

    // Accumulate middlegame and endgame scores from White's perspective.
    i32 mg_score = 0;
    i32 eg_score = 0;

    // Pawn structure
    const i32 pawn_structure =
        eval_pawn_structure(bitboard_white[PAWN], bitboard_black[PAWN], WHITE) -
        eval_pawn_structure(bitboard_black[PAWN], bitboard_white[PAWN], BLACK);
    mg_score += pawn_structure;
    eg_score += pawn_structure;

    // White Pawns
    Mask pawn_attacks_w = 0;
    Mask w_pawns = bitboard_white[PAWN];
    while (w_pawns) {
        Pos board_pos = pop_pos(w_pawns);
        pawn_attacks_w |= white_pawn_atk_mask(board_pos);
        Pos pos = invert_pos(board_pos);
        mg_score += mg_piece_value[PAWN] + mg_pawn_table[pos];
        eg_score += eg_piece_value[PAWN] + eg_pawn_table[pos];
    }

    // Black Pawns
    Mask pawn_attacks_b = 0;
    Mask b_pawns = bitboard_black[PAWN];
    while (b_pawns) {
        Pos pos = pop_pos(b_pawns);
        pawn_attacks_b |= black_pawn_atk_mask(pos);
        mg_score -= mg_piece_value[PAWN] + mg_pawn_table[pos];
        eg_score -= eg_piece_value[PAWN] + eg_pawn_table[pos];
    }

    // King positions/mask
    const Pos kings[2] = {Pos(__builtin_ctzll(bitboard_white[KING])), Pos(__builtin_ctzll(bitboard_black[KING]))};
    const Mask zones[2] = {calc_king_zone<WHITE>(kings[0]), calc_king_zone<BLACK>(kings[1])};

    const Mask occupancy = bitboard_white.occupancy() | bitboard_black.occupancy();

    // Inspiration from old stockfish
    // Mask for area where we can have useful movement
    const Mask mobility_area_white = ~(bitboard_white[PAWN] | pawn_attacks_b | bitboard_white[KING]);
    const Mask mobility_area_black = ~(bitboard_black[PAWN] | pawn_attacks_w | bitboard_black[KING]);

    // Shield bonus/penalty and attacker counts + weights
    const i32 shields[2] = {eval_pawn_shield(game.players, kings[0], WHITE), eval_pawn_shield(game.players, kings[1], BLACK)};
    i32 counts[2] = {};
    i32 weights[2] = {};

    // ------------ White ------------

    // White Knights
    while (bitboard_white[KNIGHT]) {
        Pos board_pos = pop_pos(bitboard_white[KNIGHT]);
        Mask attack = knight_atk_mask(board_pos);

        const auto& mob_bonus = KNIGHT_MOBILITY_BONUS[__builtin_popcountll(attack & mobility_area_white)];

        add_king_pressure<KNIGHT>(zones[BLACK], attack, counts[BLACK], weights[BLACK]);

        Pos pos = invert_pos(board_pos);
        mg_score += mg_piece_value[KNIGHT] + mg_knight_table[pos] + mob_bonus[0];
        eg_score += eg_piece_value[KNIGHT] + eg_knight_table[pos] + mob_bonus[1];
    }

    // White Bishops
    u8 num_w_bishops = 0;
    while (bitboard_white[BISHOP]) {
        Pos board_pos = pop_pos(bitboard_white[BISHOP]);
        Mask attack = bishop_atk_mask(occupancy ^ b_queens, board_pos);

        const auto& mob_bonus = BISHOP_MOBILITY_BONUS[__builtin_popcountll(attack & mobility_area_white)];

        add_king_pressure<BISHOP>(zones[BLACK], attack, counts[BLACK], weights[BLACK]);

        Pos pos = invert_pos(board_pos);
        mg_score += mg_piece_value[BISHOP] + mg_bishop_table[pos] + mob_bonus[0];
        eg_score += eg_piece_value[BISHOP] + eg_bishop_table[pos] + mob_bonus[1];
        num_w_bishops++;
    }
    if (num_w_bishops >= 2) {
        mg_score += BISHOP_PAIR_BONUS;
        eg_score += BISHOP_PAIR_BONUS;
    }

    // White Rooks
    while (bitboard_white[ROOK]) {
        Pos board_pos = pop_pos(bitboard_white[ROOK]);
        Mask attack = rook_atk_mask(occupancy ^ b_queens ^ w_rooks, board_pos);

        const auto& mob_bonus = ROOK_MOBILITY_BONUS[__builtin_popcountll(attack & mobility_area_white)];

        add_king_pressure<ROOK>(zones[BLACK], attack, counts[BLACK], weights[BLACK]);

        Pos pos = invert_pos(board_pos);
        mg_score += mg_piece_value[ROOK] + mg_rook_table[pos] + mob_bonus[0];
        eg_score += eg_piece_value[ROOK] + eg_rook_table[pos] + mob_bonus[1];

        if (!(all_pawns & col_mask(pos % 8))) {
            mg_score += ROOK_OPEN_FILE_BONUS;
            eg_score += ROOK_OPEN_FILE_BONUS;
        } else if (!(bitboard_white[PAWN] & col_mask(pos % 8))) {
            mg_score += ROOK_SEMI_OPEN_FILE_BONUS;
            eg_score += ROOK_SEMI_OPEN_FILE_BONUS;
        }
    }

    // White Queens
    while (bitboard_white[QUEEN]) {
        Pos board_pos = pop_pos(bitboard_white[QUEEN]);
        Mask attack = queen_atk_mask(occupancy, board_pos);

        const auto& mob_bonus = QUEEN_MOBILITY_BONUS[__builtin_popcountll(attack & mobility_area_white)];

        add_king_pressure<QUEEN>(zones[BLACK], attack, counts[BLACK], weights[BLACK]);

        Pos pos = invert_pos(board_pos);
        mg_score += mg_piece_value[QUEEN] + mg_queen_table[pos] + mob_bonus[0];
        eg_score += eg_piece_value[QUEEN] + eg_queen_table[pos] + mob_bonus[1];

        if (!(all_pawns & col_mask(pos % 8))) {
            mg_score += QUEEN_OPEN_FILE_BONUS;
            eg_score += QUEEN_OPEN_FILE_BONUS;
        } else if (!(bitboard_white[PAWN] & col_mask(pos % 8))) {
            mg_score += QUEEN_SEMI_OPEN_FILE_BONUS;
            eg_score += QUEEN_SEMI_OPEN_FILE_BONUS;
        }
    }

    Pos king_pos_w = invert_pos(__builtin_ctzll(bitboard_white[KING]));
    mg_score += mg_king_table[king_pos_w];
    eg_score += eg_king_table[king_pos_w];

    // ------------ Black -----------------------

    // Black Knights
    while (bitboard_black[KNIGHT]) {
        Pos board_pos = pop_pos(bitboard_black[KNIGHT]);
        Mask attack = knight_atk_mask(board_pos);

        const auto& mob_bonus = KNIGHT_MOBILITY_BONUS[__builtin_popcountll(attack & mobility_area_black)];

        add_king_pressure<KNIGHT>(zones[WHITE], attack, counts[WHITE], weights[WHITE]);
        Pos pos = board_pos;
        mg_score -= mg_piece_value[KNIGHT] + mg_knight_table[pos] + mob_bonus[0];
        eg_score -= eg_piece_value[KNIGHT] + eg_knight_table[pos] + mob_bonus[1];
    }

    // Black Bishops
    u8 num_b_bishops = 0;
    while (bitboard_black[BISHOP]) {
        Pos board_pos = pop_pos(bitboard_black[BISHOP]);
        Mask attack = bishop_atk_mask(occupancy ^ w_queens, board_pos);

        const auto& mob_bonus = BISHOP_MOBILITY_BONUS[__builtin_popcountll(attack & mobility_area_black)];

        add_king_pressure<BISHOP>(zones[WHITE], attack, counts[WHITE], weights[WHITE]);
        Pos pos = board_pos;
        mg_score -= mg_piece_value[BISHOP] + mg_bishop_table[pos] + mob_bonus[0];
        eg_score -= eg_piece_value[BISHOP] + eg_bishop_table[pos] + mob_bonus[1];
        num_b_bishops++;
    }
    if (num_b_bishops >= 2) {
        mg_score -= BISHOP_PAIR_BONUS;
        eg_score -= BISHOP_PAIR_BONUS;
    }

    // Black Rooks
    while (bitboard_black[ROOK]) {
        Pos board_pos = pop_pos(bitboard_black[ROOK]);
        Mask attack = rook_atk_mask(occupancy ^ w_queens ^ b_rooks, board_pos);

        const auto& mob_bonus = ROOK_MOBILITY_BONUS[__builtin_popcountll(attack & mobility_area_black)];

        add_king_pressure<ROOK>(zones[WHITE], attack, counts[WHITE], weights[WHITE]);
        Pos pos = board_pos;
        mg_score -= mg_piece_value[ROOK] + mg_rook_table[pos] + mob_bonus[0];
        eg_score -= eg_piece_value[ROOK] + eg_rook_table[pos] + mob_bonus[1];

        if (!(all_pawns & col_mask(pos % 8))) {
            mg_score -= ROOK_OPEN_FILE_BONUS;
            eg_score -= ROOK_OPEN_FILE_BONUS;
        } else if (!(bitboard_black[PAWN] & col_mask(pos % 8))) {
            mg_score -= ROOK_SEMI_OPEN_FILE_BONUS;
            eg_score -= ROOK_SEMI_OPEN_FILE_BONUS;
        }
    }

    // Black Queens
    while (bitboard_black[QUEEN]) {
        Pos board_pos = pop_pos(bitboard_black[QUEEN]);
        Mask attack = queen_atk_mask(occupancy, board_pos);

        const auto& mob_bonus = QUEEN_MOBILITY_BONUS[__builtin_popcountll(attack & mobility_area_black)];

        add_king_pressure<QUEEN>(zones[WHITE], attack, counts[WHITE], weights[WHITE]);
        Pos pos = board_pos;
        mg_score -= mg_piece_value[QUEEN] + mg_queen_table[pos] + mob_bonus[0];
        eg_score -= eg_piece_value[QUEEN] + eg_queen_table[pos] + mob_bonus[1];

        if (!(all_pawns & col_mask(pos % 8))) {
            mg_score -= QUEEN_OPEN_FILE_BONUS;
            eg_score -= QUEEN_OPEN_FILE_BONUS;
        } else if (!(bitboard_black[PAWN] & col_mask(pos % 8))) {
            mg_score -= QUEEN_SEMI_OPEN_FILE_BONUS;
            eg_score -= QUEEN_SEMI_OPEN_FILE_BONUS;
        }
    }

    Pos king_pos_b = __builtin_ctzll(bitboard_black[KING]);
    mg_score -= mg_king_table[king_pos_b];
    eg_score -= eg_king_table[king_pos_b];

    const i32 wp = weights[WHITE] * coordination_mult[std::min(counts[WHITE], 5)] / 100;
    const i32 bp = weights[BLACK] * coordination_mult[std::min(counts[BLACK], 5)] / 100;
    mg_score += shields[WHITE] - safety_table[std::min(wp, 99)];
    mg_score -= shields[BLACK] - safety_table[std::min(bp, 99)];

    const i32 score = static_cast<i32>(
        (1.0f - endgame) * mg_score + endgame * eg_score);

    return (game.turn == WHITE) ? score : -score;
}


i32 eval_game(Game& game) {
    const f32 endgame = endgame_ratio(game);

    Bitboard bitboard_white = game.players[WHITE].bb;
    Bitboard bitboard_black = game.players[BLACK].bb;

    const Mask w_rooks = bitboard_white[ROOK];
    const Mask b_rooks = bitboard_black[ROOK];
    const Mask w_queens = bitboard_white[QUEEN];
    const Mask b_queens = bitboard_black[QUEEN];

    const Mask all_pawns = bitboard_white[PAWN] | bitboard_black[PAWN];

    // Accumulate middlegame and endgame scores from White's perspective.
    i32 mg_score = 0;
    i32 eg_score = 0;

    // Pawn structure
    const i32 pawn_structure =
        eval_pawn_structure(bitboard_white[PAWN], bitboard_black[PAWN], WHITE) -
        eval_pawn_structure(bitboard_black[PAWN], bitboard_white[PAWN], BLACK);
    mg_score += pawn_structure;
    eg_score += pawn_structure;

    // White Pawns
    Mask pawn_attacks_w = 0;
    Mask w_pawns = bitboard_white[PAWN];
    while (w_pawns) {
        Pos board_pos = pop_pos(w_pawns);
        pawn_attacks_w |= white_pawn_atk_mask(board_pos);
        Pos pos = invert_pos(board_pos);
        mg_score += mg_piece_value[PAWN] + mg_pawn_table[pos];
        eg_score += eg_piece_value[PAWN] + eg_pawn_table[pos];
    }

    // Black Pawns
    Mask pawn_attacks_b = 0;
    Mask b_pawns = bitboard_black[PAWN];
    while (b_pawns) {
        Pos pos = pop_pos(b_pawns);
        pawn_attacks_b |= black_pawn_atk_mask(pos);
        mg_score -= mg_piece_value[PAWN] + mg_pawn_table[pos];
        eg_score -= eg_piece_value[PAWN] + eg_pawn_table[pos];
    }

    // King positions/mask
    const Pos kings[2] = {Pos(__builtin_ctzll(bitboard_white[KING])), Pos(__builtin_ctzll(bitboard_black[KING]))};
    const Mask zones[2] = {calc_king_zone<WHITE>(kings[0]), calc_king_zone<BLACK>(kings[1])};

    const Mask occupancy = bitboard_white.occupancy() | bitboard_black.occupancy();

    // Inspiration from old stockfish
    // Mask for area where we can have useful movement
    const Mask mobility_area_white = ~(bitboard_white[PAWN] | pawn_attacks_b | bitboard_white[KING]);
    const Mask mobility_area_black = ~(bitboard_black[PAWN] | pawn_attacks_w | bitboard_black[KING]);

    // Shield bonus/penalty and attacker counts + weights
    const i32 shields[2] = {eval_pawn_shield(game.players, kings[0], WHITE), eval_pawn_shield(game.players, kings[1], BLACK)};
    i32 counts[2] = {};
    i32 weights[2] = {};

    // ------------ White ------------

    // White Knights
    while (bitboard_white[KNIGHT]) {
        Pos board_pos = pop_pos(bitboard_white[KNIGHT]);
        Mask attack = knight_atk_mask(board_pos);

        const auto& mob_bonus = KNIGHT_MOBILITY_BONUS[__builtin_popcountll(attack & mobility_area_white)];

        add_king_pressure<KNIGHT>(zones[BLACK], attack, counts[BLACK], weights[BLACK]);

        Pos pos = invert_pos(board_pos);
        mg_score += mg_piece_value[KNIGHT] + mg_knight_table[pos] + mob_bonus[0];
        eg_score += eg_piece_value[KNIGHT] + eg_knight_table[pos] + mob_bonus[1];
    }

    // White Bishops
    u8 num_w_bishops = 0;
    while (bitboard_white[BISHOP]) {
        Pos board_pos = pop_pos(bitboard_white[BISHOP]);
        Mask attack = bishop_atk_mask(occupancy ^ b_queens, board_pos);

        const auto& mob_bonus = BISHOP_MOBILITY_BONUS[__builtin_popcountll(attack & mobility_area_white)];

        add_king_pressure<BISHOP>(zones[BLACK], attack, counts[BLACK], weights[BLACK]);

        Pos pos = invert_pos(board_pos);
        mg_score += mg_piece_value[BISHOP] + mg_bishop_table[pos] + mob_bonus[0];
        eg_score += eg_piece_value[BISHOP] + eg_bishop_table[pos] + mob_bonus[1];
        num_w_bishops++;
    }
    if (num_w_bishops >= 2) {
        mg_score += BISHOP_PAIR_BONUS;
        eg_score += BISHOP_PAIR_BONUS;
    }

    // White Rooks
    while (bitboard_white[ROOK]) {
        Pos board_pos = pop_pos(bitboard_white[ROOK]);
        Mask attack = rook_atk_mask(occupancy ^ b_queens ^ w_rooks, board_pos);

        const auto& mob_bonus = ROOK_MOBILITY_BONUS[__builtin_popcountll(attack & mobility_area_white)];

        add_king_pressure<ROOK>(zones[BLACK], attack, counts[BLACK], weights[BLACK]);

        Pos pos = invert_pos(board_pos);
        mg_score += mg_piece_value[ROOK] + mg_rook_table[pos] + mob_bonus[0];
        eg_score += eg_piece_value[ROOK] + eg_rook_table[pos] + mob_bonus[1];

        if (!(all_pawns & col_mask(pos % 8))) {
            mg_score += ROOK_OPEN_FILE_BONUS;
            eg_score += ROOK_OPEN_FILE_BONUS;
        } else if (!(bitboard_white[PAWN] & col_mask(pos % 8))) {
            mg_score += ROOK_SEMI_OPEN_FILE_BONUS;
            eg_score += ROOK_SEMI_OPEN_FILE_BONUS;
        }
    }

    // White Queens
    while (bitboard_white[QUEEN]) {
        Pos board_pos = pop_pos(bitboard_white[QUEEN]);
        Mask attack = queen_atk_mask(occupancy, board_pos);

        const auto& mob_bonus = QUEEN_MOBILITY_BONUS[__builtin_popcountll(attack & mobility_area_white)];

        add_king_pressure<QUEEN>(zones[BLACK], attack, counts[BLACK], weights[BLACK]);

        Pos pos = invert_pos(board_pos);
        mg_score += mg_piece_value[QUEEN] + mg_queen_table[pos] + mob_bonus[0];
        eg_score += eg_piece_value[QUEEN] + eg_queen_table[pos] + mob_bonus[1];

        if (!(all_pawns & col_mask(pos % 8))) {
            mg_score += QUEEN_OPEN_FILE_BONUS;
            eg_score += QUEEN_OPEN_FILE_BONUS;
        } else if (!(bitboard_white[PAWN] & col_mask(pos % 8))) {
            mg_score += QUEEN_SEMI_OPEN_FILE_BONUS;
            eg_score += QUEEN_SEMI_OPEN_FILE_BONUS;
        }
    }

    Pos king_pos_w = invert_pos(__builtin_ctzll(bitboard_white[KING]));
    mg_score += mg_king_table[king_pos_w];
    eg_score += eg_king_table[king_pos_w];

    // ------------ Black -----------------------

    // Black Knights
    while (bitboard_black[KNIGHT]) {
        Pos board_pos = pop_pos(bitboard_black[KNIGHT]);
        Mask attack = knight_atk_mask(board_pos);

        const auto& mob_bonus = KNIGHT_MOBILITY_BONUS[__builtin_popcountll(attack & mobility_area_black)];

        add_king_pressure<KNIGHT>(zones[WHITE], attack, counts[WHITE], weights[WHITE]);
        Pos pos = board_pos;
        mg_score -= mg_piece_value[KNIGHT] + mg_knight_table[pos] + mob_bonus[0];
        eg_score -= eg_piece_value[KNIGHT] + eg_knight_table[pos] + mob_bonus[1];
    }

    // Black Bishops
    u8 num_b_bishops = 0;
    while (bitboard_black[BISHOP]) {
        Pos board_pos = pop_pos(bitboard_black[BISHOP]);
        Mask attack = bishop_atk_mask(occupancy ^ w_queens, board_pos);

        const auto& mob_bonus = BISHOP_MOBILITY_BONUS[__builtin_popcountll(attack & mobility_area_black)];

        add_king_pressure<BISHOP>(zones[WHITE], attack, counts[WHITE], weights[WHITE]);
        Pos pos = board_pos;
        mg_score -= mg_piece_value[BISHOP] + mg_bishop_table[pos] + mob_bonus[0];
        eg_score -= eg_piece_value[BISHOP] + eg_bishop_table[pos] + mob_bonus[1];
        num_b_bishops++;
    }
    if (num_b_bishops >= 2) {
        mg_score -= BISHOP_PAIR_BONUS;
        eg_score -= BISHOP_PAIR_BONUS;
    }

    // Black Rooks
    while (bitboard_black[ROOK]) {
        Pos board_pos = pop_pos(bitboard_black[ROOK]);
        Mask attack = rook_atk_mask(occupancy ^ w_queens ^ b_rooks, board_pos);

        const auto& mob_bonus = ROOK_MOBILITY_BONUS[__builtin_popcountll(attack & mobility_area_black)];

        add_king_pressure<ROOK>(zones[WHITE], attack, counts[WHITE], weights[WHITE]);
        Pos pos = board_pos;
        mg_score -= mg_piece_value[ROOK] + mg_rook_table[pos] + mob_bonus[0];
        eg_score -= eg_piece_value[ROOK] + eg_rook_table[pos] + mob_bonus[1];

        if (!(all_pawns & col_mask(pos % 8))) {
            mg_score -= ROOK_OPEN_FILE_BONUS;
            eg_score -= ROOK_OPEN_FILE_BONUS;
        } else if (!(bitboard_black[PAWN] & col_mask(pos % 8))) {
            mg_score -= ROOK_SEMI_OPEN_FILE_BONUS;
            eg_score -= ROOK_SEMI_OPEN_FILE_BONUS;
        }
    }

    // Black Queens
    while (bitboard_black[QUEEN]) {
        Pos board_pos = pop_pos(bitboard_black[QUEEN]);
        Mask attack = queen_atk_mask(occupancy, board_pos);

        const auto& mob_bonus = QUEEN_MOBILITY_BONUS[__builtin_popcountll(attack & mobility_area_black)];

        add_king_pressure<QUEEN>(zones[WHITE], attack, counts[WHITE], weights[WHITE]);
        Pos pos = board_pos;
        mg_score -= mg_piece_value[QUEEN] + mg_queen_table[pos] + mob_bonus[0];
        eg_score -= eg_piece_value[QUEEN] + eg_queen_table[pos] + mob_bonus[1];

        if (!(all_pawns & col_mask(pos % 8))) {
            mg_score -= QUEEN_OPEN_FILE_BONUS;
            eg_score -= QUEEN_OPEN_FILE_BONUS;
        } else if (!(bitboard_black[PAWN] & col_mask(pos % 8))) {
            mg_score -= QUEEN_SEMI_OPEN_FILE_BONUS;
            eg_score -= QUEEN_SEMI_OPEN_FILE_BONUS;
        }
    }

    Pos king_pos_b = __builtin_ctzll(bitboard_black[KING]);
    mg_score -= mg_king_table[king_pos_b];
    eg_score -= eg_king_table[king_pos_b];

    const i32 wp = weights[WHITE] * coordination_mult[std::min(counts[WHITE], 5)] / 100;
    const i32 bp = weights[BLACK] * coordination_mult[std::min(counts[BLACK], 5)] / 100;
    mg_score += shields[WHITE] - safety_table[std::min(wp, 99)];
    mg_score -= shields[BLACK] - safety_table[std::min(bp, 99)];

    const i32 score = static_cast<i32>(
        (1.0f - endgame) * mg_score + endgame * eg_score);

    return (game.turn == WHITE) ? score : -score;
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