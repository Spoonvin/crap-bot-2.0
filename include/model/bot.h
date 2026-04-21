#pragma once

#include "model.h"
#include "search/search.h"

struct Bot : Model {

  Searcher searcher;

  Bot();
  
  Move select_best(Game& game) override;

};