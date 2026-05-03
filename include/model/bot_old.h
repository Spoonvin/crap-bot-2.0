#pragma once

#include "model.h"
#include "search/search.h"

struct BotOld : Model {

  Searcher searcher;

  BotOld();
  BotOld(u32 time_lim);
  
  Move select_best(Game& game) override;

};