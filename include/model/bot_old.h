#pragma once

#include "model.h"
#include "search/search_old.h"

struct BotOld : Model {

  SearcherOld searcher;

  BotOld();
  BotOld(u32 time_lim);
  
  Move select_best(Game& game) override;

};