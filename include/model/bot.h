#pragma once

#include "model.h"
#include "search/search.h"

struct Bot : Model {
  
  Move select_best(Game& game) override;

};