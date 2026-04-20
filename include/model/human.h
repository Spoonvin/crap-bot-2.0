#pragma once

#include "model.h"

struct Human : Model {
  Move select_best(Game& game) override;
};