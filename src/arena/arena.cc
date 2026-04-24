#include "arena/arena.h"
#include "model/model.h"
#include "gui/gui.h"
#include "chess/move/movegen.h"
#include "search/zobrist_hash.h"

#include <iostream>

Arena::Arena(Game* game, Model* white, Model* black) {
    this->game = game;
    white_player = white;
    black_player = black;
}

ArenaResult Arena::play(){
    while (true) {

        Move move;

        if (game->turn == WHITE) {
            move = white_player->select_best(*game);
        } else {
            move = black_player->select_best(*game);
        }

        // Make move
        game->make_move(move);

        MoveList moves;
        GenResult gen_result = gen_legal(*game, moves);

        if (gen_result.count <= 0 && gen_result.check != NO_CHECK) {
            if (game->turn == WHITE) {
                return BLACK_WIN;
            } else {
                return WHITE_WIN;
            }
        }

        if (gen_result.count <= 0) {
            return DRAW;
        }

        if (gui_ctx.is_init()) {
            gui_ctx.render_game(*game, move.to());
            gui_ctx.present();
        }
    }
}
