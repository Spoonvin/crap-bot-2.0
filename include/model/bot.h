#pragma once

#include "model.h"
#include "search/search.h"

struct Bot : Model {

  Searcher searcher;

  Bot();
  Bot(u32 time_lim);
  
  Move select_best(Game& game) override;

};