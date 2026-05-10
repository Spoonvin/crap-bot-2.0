#include "arena/arena.h"
#include "model/model.h"
#include "chess/move/movegen.h"
#include "search/zobrist_hash.h"
#include "model/bot.h"

#include <iostream>

#ifndef DUSE_GUI
#define DUSE_GUI 1
#endif

#if DUSE_GUI
#include "gui/gui.h"
#endif

Arena::Arena(Game game, Model* white, Model* black) {
    this->game = game;
    white_player = white;
    black_player = black;
}

ArenaResult Arena::play(){

    #if DUSE_GUI
    gui_ctx.init(600);

    if (gui_ctx.is_init()) {
        gui_ctx.render_game(this->game, -1);
        gui_ctx.present();
    }
    #endif

    ArenaResult result = DRAW;

    while (true) {

        MoveList moves;
        GenResult gen_result = gen_legal(game, moves);

        if (gen_result.count <= 0 && gen_result.check != NO_CHECK) {
            if (game.turn == WHITE) {
                result = BLACK_WIN;
                break;
            } else {
                result = WHITE_WIN;
                break;
            }
        }

        if (gen_result.count <= 0 || game.is_draw()) {
            result = DRAW;
            break;
        }

        Move move;

        if (game.turn == WHITE) {
            move = white_player->select_best(game);
        } else {
            move = black_player->select_best(game);
        }

        if (move.is_null()) {
            char fen[MAX_FEN] = {};
            game.store_fen(fen);
            std::cout << fen << "\n";
        }

        assert(!move.is_null());

        // Make move
        game.make_move(move);

        #if DUSE_GUI
        if (gui_ctx.is_init()) {
            gui_ctx.render_game(game, move.to());
            gui_ctx.present();
        }
        #endif
    }

    #if DUSE_GUI
    gui_ctx.quit();
    #endif

    return result;
}
