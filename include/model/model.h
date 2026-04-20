#pragma once

#include "chess/game.h"

struct Model {
  virtual Move select_best(Game& game) = 0;
  virtual ~Model() = default;
};