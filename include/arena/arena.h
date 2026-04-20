#pragma once

#include "chess/game.h"
#include "model/model.h"

enum ArenaResult{
    WHITE_WIN,
    BLACK_WIN,
    DRAW
};

struct Arena{
    Game* game;
    Model* white_player;
    Model* black_player;

    Arena(Game* game, Model* white, Model* black);

    ArenaResult play();
};