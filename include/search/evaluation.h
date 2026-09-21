#pragma once
#include "common/aliases.h"
#include "chess/game.h"

#define PAWN_VALUE 100
#define KNIGHT_VALUE 320
#define BISHOP_VALUE 330
#define ROOK_VALUE 500
#define QUEEN_VALUE 900
#define KING_VALUE 1000 // For move ordering

#define KNIGHT_VALUE_OLD 300
#define BISHOP_VALUE_OLD 300

#define MATE_VALUE 999999

#define MAX_VALUE (MATE_VALUE + 100)
#define MIN_VALUE -(MAX_VALUE)


i32 eval_game(Game& game);
i32 eval_game_old(Game& game);

// Start = 0, king n pawns = 1, [0,1].
f32 endgame_ratio(const Game& game);

i32 square_value(Square square);

// Check the position before making the move; excludes captures and promotions.
bool is_quiet(Move move, Game& game);

i32 calc_move_score(Move move, Game& game);
i32 calc_move_score_old(Move move, Game& game);


i32 eval_pawn_structure(Mask acting_pawns, Mask opponent_pawns, Color acting_color);

// Pawn shelter and open approaches, in middlegame centipawns for acting_color.
// king_pos uses board coordinates; eval_game applies the phase weight.
i32 eval_king_safety(const Player players[2], Pos king_pos, Color acting_color);
