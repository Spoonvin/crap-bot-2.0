#pragma once

#include "chess/game.h"

struct Model {
  virtual Move select_best(Game& game) = 0;
  virtual void clear_transposition_table() {}
  virtual ~Model() = default;
};
