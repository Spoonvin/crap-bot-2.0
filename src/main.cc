#include <cstdio>
#include <cstring>
#include <iostream>

#include "gui/gui.h"
#include "model/human.h"
#include "model/bot.h"
#include "search/evaluation.h"
#include "search/search.h"
#include "arena/arena.h"

i32 main(i32 argc, char** argv) {

  gui_ctx.init(600);

  Game game;
  game = Game::initial();


  if (gui_ctx.is_init()) {
    gui_ctx.render_game(game, -1);
    gui_ctx.present();
  }

  Bot* bot = new Bot();
  Human* human = new Human();

  //Arena arena(&game, human, bot);

  //ArenaResult result = arena.play();

  bot->select_best(game);

  gui_ctx.quit();
  return 0;
}
