#include <cstdio>
#include <cstring>
#include <iostream>

#include "gui/gui.h"
#include "model/human.h"
#include "model/bot.h"
#include "model/bot_old.h"
#include "search/evaluation.h"
#include "search/search.h"
#include "arena/arena.h"
#include "search/zobrist_hash.h"
#include "arena/benchmark.h"
#include "search/opening_book.h"

typedef void (*CommandFunction)(i32 argc, char** argv);

struct Command {
  const char* name;
  const char* usage;
  CommandFunction function;
};

void play(i32 argc, char** argv) {

  Game game;
  game = Game::initial();
  Human* human = new Human();

  Bot* bot= new Bot();

  Model* white;
  Model* black;

  if (argc < 3) {
    std::cout << "Setting player to white\n";
    white = static_cast<Model*>(human);
    black = static_cast<Model*>(bot);
  } else if (strcmp(argv[2], "white") == 0) {
    white = static_cast<Model*>(human);
    black = static_cast<Model*>(bot);
  } else if (strcmp(argv[2], "black") == 0) {
    white = static_cast<Model*>(bot);
    black = static_cast<Model*>(human);
  }

  Arena arena(game, white, black);
  arena.play();

  delete white;
  delete black;

  return;
}

void benchmark(i32 argc, char** argv) {

  i32 iterations = std::stoi(argv[2]);

  Game game;
  game = Game::initial();

  Bot* bot_new= new Bot();
  BotOld* bot_old = new BotOld();

  Benchmark bench(bot_new, bot_old);

  BenchmarkStats stats = bench.run_benchmark(game, iterations);

  std::cout << "----- Benchmark Final Result -----\n";
  std::cout << "Bot wins: " << stats.m1_score << "\n";
  std::cout << "Old bot wins: " << stats.m2_score << "\n";
  std::cout << "Draws: " << stats.draws << "\n";
  
  delete bot_new;
  delete bot_old;

}

void test_fen(i32 argc, char** argv) {

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
  {"test_fen", "test_fen <FEN> <search-time> : print the best move from position.", test_fen},
  {"benchmark", "benchmark <iterations> : benchmark bot against old bot.", benchmark}
};

const int numCommands = sizeof(commands) / sizeof(Command);

i32 main(i32 argc, char** argv) {

  if (argc < 2) {
    std::cout << "Specify command\n";
    return 1;
  } 

  init_hash_key_map();

  const char* command_name = argv[1];
  const char* options = (argc > 2) ? argv[2] : "";

  for (int i = 0; i < numCommands; ++i) {
    if (strcmp(command_name, commands[i].name) == 0) {
      commands[i].function(argc, argv);
      return 0;
    }
  }

  std::cout << "Unknown command: " << command_name << "\n";
  return 1;
}

