#include "search/evaluation.h"
#include "chess/game.h"
#include "chess/board/mask_operations.h"

#include <iostream>

#define DOUBLE_PAWN_PENALTY 20
#define ISOLATED_PAWN_PENALTY 25

Mask WHITE_PASSED_TABLE[64];
Mask BLACK_PASSED_TABLE[64];
i32 PASSED_PAWN_BONUS[8] = {0, 5, 10, 20, 40, 70, 100, 0};

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
    50, 100,60, 80, 70, 100,20,  0,
    10, 10, 20, 30, 30, 20, 10,  0,
    5,  5, 10, 25, 25, 10,  5,  0,
    0,  0,  0, 20, 20,  0,  0,  0,
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
    Bitboard bitboard_white = game.players[WHITE].bb;
    Bitboard bitboard_black = game.players[BLACK].bb;

    i32 white_score = 0;
    i32 black_score = 0;

    // White Pawns
    while (bitboard_white[PAWN]) {
        Pos pos = invert_pos(pop_pos(bitboard_white[PAWN]));
        white_score += PAWN_VALUE + b_pawn_square_mod[pos];
    }

    // White Knights
    while (bitboard_white[KNIGHT]) {
        Pos pos = invert_pos(pop_pos(bitboard_white[KNIGHT]));
        white_score += KNIGHT_VALUE + b_knight_square_mod[pos];
    }

    // White Bishops
    while (bitboard_white[BISHOP]) {
        Pos pos = invert_pos(pop_pos(bitboard_white[BISHOP]));
        white_score += BISHOP_VALUE + b_bishop_square_mod[pos];
    }

    // White Rooks
    while (bitboard_white[ROOK]) {
        Pos pos = invert_pos(pop_pos(bitboard_white[ROOK]));
        white_score += ROOK_VALUE + b_rook_square_mod[pos];
    }

    // White Queens
    while (bitboard_white[QUEEN]) {
        Pos pos = invert_pos(pop_pos(bitboard_white[QUEEN]));
        white_score += QUEEN_VALUE + b_queen_square_mod[pos];
    }

    Pos king_pos_w = invert_pos(__builtin_ctzll(bitboard_white[KING]));
    white_score += b_king_square_mod[king_pos_w];

    // Black -------------------------------

    // Black Pawns
    while (bitboard_black[PAWN]) {
        Pos pos = pop_pos(bitboard_black[PAWN]);
        black_score += PAWN_VALUE + b_pawn_square_mod[pos];
    }

    // Black Knights
    while (bitboard_black[KNIGHT]) {
        Pos pos = pop_pos(bitboard_black[KNIGHT]);
        black_score += KNIGHT_VALUE + b_knight_square_mod[pos];
    }

    // Black Bishops
    while (bitboard_black[BISHOP]) {
        Pos pos = pop_pos(bitboard_black[BISHOP]);
        black_score += BISHOP_VALUE + b_bishop_square_mod[pos];
    }

    // Black Rooks
    while (bitboard_black[ROOK]) {
        Pos pos = pop_pos(bitboard_black[ROOK]);
        black_score += ROOK_VALUE + b_rook_square_mod[pos];
    }

    // Black Queens
    while (bitboard_black[QUEEN]) {
        Pos pos = pop_pos(bitboard_black[QUEEN]);
        black_score += QUEEN_VALUE + b_queen_square_mod[pos];
    }

    Pos king_pos_b = __builtin_ctzll(bitboard_black[KING]);
    black_score += b_king_square_mod[king_pos_b];

    i32 result = (game.turn == WHITE) ? white_score - black_score : black_score - white_score;

    return result;
}

i32 eval_game(Game& game) {
    Bitboard bitboard_white = game.players[WHITE].bb;
    Bitboard bitboard_black = game.players[BLACK].bb;

    i32 white_score = 0;
    i32 black_score = 0;

    // White Pawns
    while (bitboard_white[PAWN]) {
        Pos pos = invert_pos(pop_pos(bitboard_white[PAWN]));
        white_score += PAWN_VALUE + b_pawn_square_mod[pos];
    }
    white_score += eval_pawn_structure(bitboard_white[PAWN], bitboard_black[PAWN], WHITE);

    // White Knights
    while (bitboard_white[KNIGHT]) {
        Pos pos = invert_pos(pop_pos(bitboard_white[KNIGHT]));
        white_score += KNIGHT_VALUE + b_knight_square_mod[pos];
    }

    // White Bishops
    while (bitboard_white[BISHOP]) {
        Pos pos = invert_pos(pop_pos(bitboard_white[BISHOP]));
        white_score += BISHOP_VALUE + b_bishop_square_mod[pos];
    }

    // White Rooks
    while (bitboard_white[ROOK]) {
        Pos pos = invert_pos(pop_pos(bitboard_white[ROOK]));
        white_score += ROOK_VALUE + b_rook_square_mod[pos];
    }

    // White Queens
    while (bitboard_white[QUEEN]) {
        Pos pos = invert_pos(pop_pos(bitboard_white[QUEEN]));
        white_score += QUEEN_VALUE + b_queen_square_mod[pos];
    }

    Pos king_pos_w = invert_pos(__builtin_ctzll(bitboard_white[KING]));
    white_score += b_king_square_mod[king_pos_w];

    // Black -------------------------------

    // Black Pawns
    while (bitboard_black[PAWN]) {
        Pos pos = pop_pos(bitboard_black[PAWN]);
        black_score += PAWN_VALUE + b_pawn_square_mod[pos];
    }
    black_score += eval_pawn_structure(bitboard_black[PAWN], bitboard_white[PAWN], BLACK);

    // Black Knights
    while (bitboard_black[KNIGHT]) {
        Pos pos = pop_pos(bitboard_black[KNIGHT]);
        black_score += KNIGHT_VALUE + b_knight_square_mod[pos];
    }

    // Black Bishops
    while (bitboard_black[BISHOP]) {
        Pos pos = pop_pos(bitboard_black[BISHOP]);
        black_score += BISHOP_VALUE + b_bishop_square_mod[pos];
    }

    // Black Rooks
    while (bitboard_black[ROOK]) {
        Pos pos = pop_pos(bitboard_black[ROOK]);
        black_score += ROOK_VALUE + b_rook_square_mod[pos];
    }

    // Black Queens
    while (bitboard_black[QUEEN]) {
        Pos pos = pop_pos(bitboard_black[QUEEN]);
        black_score += QUEEN_VALUE + b_queen_square_mod[pos];
    }

    Pos king_pos_b = __builtin_ctzll(bitboard_black[KING]);
    black_score += b_king_square_mod[king_pos_b];

    i32 result = (game.turn == WHITE) ? white_score - black_score : black_score - white_score;

    return result;
}

i32 square_value(Square square) {
    if (square == EMPTY_SQUARE) return 0;

    Piece piece = stp(square);

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

i32 mvv_lva_score(Move move, Game& game) {
    Pos from = move.from();
    Pos to = move.to();
    Square attacker = game.board[from];
    Square victim = game.board[to];
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