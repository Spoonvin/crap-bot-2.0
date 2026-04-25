#include "search/evaluation.h"
#include "chess/game.h"
#include <iostream>

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

Pos invert_pos(Pos pos) {
    i8 new_pos = ((pos + 56)) - ((pos / 8) * 16);
    return new_pos;
}

i32 eval_game(Game& game) {
    Bitboard bitboard_white = game.players[WHITE].bb;
    Bitboard bitboard_black = game.players[BLACK].bb;


    i32 white_score = 0;
    i32 black_score = 0;
    for (u8 piece = PAWN; piece < PIECE_COUNT; piece++) {
        switch (piece) {
            case PAWN:
                white_score += __builtin_popcountll(bitboard_white[PAWN]) * PAWN_VALUE;
                black_score += __builtin_popcountll(bitboard_black[PAWN]) * PAWN_VALUE;
                break;
            case KNIGHT:
                white_score += __builtin_popcountll(bitboard_white[KNIGHT]) * KNIGHT_VALUE;
                black_score += __builtin_popcountll(bitboard_black[KNIGHT]) * KNIGHT_VALUE;
                break;
            case BISHOP:
                white_score += __builtin_popcountll(bitboard_white[BISHOP]) * BISHOP_VALUE;
                black_score += __builtin_popcountll(bitboard_black[BISHOP]) * BISHOP_VALUE;
                break;
            case ROOK:
                white_score += __builtin_popcountll(bitboard_white[ROOK]) * ROOK_VALUE;
                black_score += __builtin_popcountll(bitboard_black[ROOK]) * ROOK_VALUE;
                break;
            case QUEEN:
                white_score += __builtin_popcountll(bitboard_white[QUEEN]) * QUEEN_VALUE;
                black_score += __builtin_popcountll(bitboard_black[QUEEN]) * QUEEN_VALUE;
                break;
            default:
                break;
        }
    }

    // Add piece-square modifiers
    for (Pos pos = 0; pos < 63; pos++) {
        Square sq = game.board[pos];
        if (sq == EMPTY_SQUARE) continue;

        Pos mod_index = (stc(sq) == WHITE) ? invert_pos(pos) : pos;
        Piece piece = stp(sq);

        i8 mod = 0;

        switch (piece) {
            case PAWN:
                mod += b_pawn_square_mod[mod_index];
                break;
            case KNIGHT:
                mod += b_knight_square_mod[mod_index];
                break;
            case BISHOP:
                mod += b_bishop_square_mod[mod_index];
                break;
            case ROOK:
                mod += b_rook_square_mod[mod_index];
                break;
            case QUEEN:
                mod += b_queen_square_mod[mod_index];
                break;
            case KING:
                mod += b_king_square_mod[mod_index];
                break;
            default:
                break;
        }

        if (stc(sq) == WHITE) {
            white_score += mod;
        } else {
            black_score += mod;
        }
    }

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

