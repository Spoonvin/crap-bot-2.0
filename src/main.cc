#include <cstdio>
#include <cstring>
#include <iostream>

#include "gui/gui.h"
#include "model/human.h"
#include "model/bot.h"
#include "search/evaluation.h"
#include "search/search.h"
#include "arena/arena.h"
#include "search/zobrist_hash.h"
#include "arena/benchmark.h"

typedef void (*CommandFunction)(char** argv);

struct Command {
  const char* name;
  const char* usage;
  CommandFunction function;
};

void play(char** argv) {

  Game game;
  game = Game::initial();

  gui_ctx.init(600);

  if (gui_ctx.is_init()) {
    gui_ctx.render_game(game, -1);
    gui_ctx.present();
  }

  Human* human = new Human();

  Bot* bot= new Bot();

  Model* white;
  Model* black;

  if (strcmp(argv[2], "white") == 0) {
    white = static_cast<Model*>(human);
    black = static_cast<Model*>(bot);
  } else if (strcmp(argv[2], "black") == 0) {
    white = static_cast<Model*>(bot);
    black = static_cast<Model*>(human);
  } else {
    std::cout << "Setting player to white\n";
    white = static_cast<Model*>(human);
    black = static_cast<Model*>(bot);
    return;
  }

  Arena arena(&game, white, black);
  arena.play();

  delete bot;
  delete human;

  gui_ctx.quit();

  return;
}

void test_fen(char** argv) {

  const char* fen = argv[2];
  u32 time_limit = (u32)std::stoi(argv[3]);

  std::cout << "Testing FEN: " << fen << " with time limit: " << time_limit << "ms\n";

  Game game;
  game = Game::from_fen(fen);

  Bot* bot= new Bot((u32)time_limit);

  Move move = bot->select_best(game);

  char buffer[6];
  move.store_alg(buffer);

  std::cout << "Best move: " << buffer << "\n";

}

const Command commands[] = {
  {"play", "play <color> : play vs the bot.", play},
  {"test_fen", "test_fen <FEN> <search-time> : print the best move from position.", test_fen}
};

const int numCommands = sizeof(commands) / sizeof(Command);

i32 main(i32 argc, char** argv) {

  if (argc < 2) {
    std::cout << "Usage: " << argv[0] << " [command] [options]\n";
    std::cout << "Available commands:\n";
    for (int i = 0; i < numCommands; ++i) {
      std::cout << "  " << commands[i].name << ": " << commands[i].usage << "\n";
    }
    return 1;
  } 

  init_hash_key_map();

  const char* command_name = argv[1];
  const char* options = (argc > 2) ? argv[2] : "";

  for (int i = 0; i < numCommands; ++i) {
    if (strcmp(command_name, commands[i].name) == 0) {
      commands[i].function(argv);
      return 0;
    }
  }

  std::cout << "Unknown command: " << command_name << "\n";
  return 1;
}

