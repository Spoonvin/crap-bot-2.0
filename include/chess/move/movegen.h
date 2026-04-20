#pragma once

#include <cstdio>
#include <cstring>

#include "chess/game.h"

using MoveList = Move[256];

const size_t MOVE_LIST_SIZE = sizeof(MoveList) / sizeof(Move);

enum Check : i8 {
  NO_CHECK,
  DIRECT_CHECK,
  SLIDE_CHECK,
  DOUBLE_CHECK,
};

struct GenResult {
  u8 count;
  Check check;
};

GenResult gen_legal(Game& game, MoveList& buffer);

GenResult gen_non_quiet(Game& game, MoveList& buffer);

