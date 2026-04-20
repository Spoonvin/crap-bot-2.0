#pragma once
#include "common/aliases.h"
#include "chess/game.h"

#define PAWN_VALUE 100
#define KNIGHT_VALUE 300
#define BISHOP_VALUE 300
#define ROOK_VALUE 500
#define QUEEN_VALUE 900
#define KING_VALUE 1000 // For move ordering


#define MATE_VALUE 999999

#define MAX_VALUE (MATE_VALUE + 1)
#define MIN_VALUE -(MAX_VALUE)


i32 eval_game(Game& game);

i32 square_value(Square square);

i32 mvv_lva_score(Move move, Game& game);