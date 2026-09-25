#include "UCI/uci.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#include "chess/game.h"
#include "chess/move/movegen.h"
#include "search/search.h"

namespace {

constexpr unsigned int DEFAULT_MOVE_TIME_MS = 800;
constexpr unsigned int MIN_SEARCH_TIME_MS = 100;
constexpr unsigned int DEFAULT_MOVE_OVERHEAD_MS = 100;
constexpr unsigned int MAX_MOVE_OVERHEAD_MS = 5000;

std::mutex output_mutex;

void send(const std::string& message) {
  std::lock_guard<std::mutex> lock(output_mutex);
  std::cout << message << '\n' << std::flush;
}

bool is_decimal(const std::string& value) {
  return !value.empty() && std::all_of(value.begin(), value.end(),
      [](unsigned char c) { return std::isdigit(c); });
}

bool is_valid_fen(const std::string& fen) {
  std::istringstream input(fen);
  std::string board, side, castling, ep, halfmove, fullmove, extra;
  if (!(input >> board >> side >> castling >> ep >> halfmove >> fullmove) ||
      (input >> extra)) {
    return false;
  }

  int rank_count = 1;
  int files_in_rank = 0;
  int white_kings = 0;
  int black_kings = 0;
  for (char c : board) {
    if (c == '/') {
      if (files_in_rank != 8 || ++rank_count > 8) return false;
      files_in_rank = 0;
    } else if (c >= '1' && c <= '8') {
      files_in_rank += c - '0';
    } else if (std::string("prnbqkPRNBQK").find(c) != std::string::npos) {
      ++files_in_rank;
      white_kings += c == 'K';
      black_kings += c == 'k';
    } else {
      return false;
    }
    if (files_in_rank > 8) return false;
  }
  if (rank_count != 8 || files_in_rank != 8 || white_kings != 1 ||
      black_kings != 1 ||
      (side != "w" && side != "b") || !is_decimal(halfmove) ||
      !is_decimal(fullmove)) {
    return false;
  }

  if (castling != "-") {
    std::string seen;
    for (char c : castling) {
      if (std::string("KQkq").find(c) == std::string::npos ||
          seen.find(c) != std::string::npos) {
        return false;
      }
      seen += c;
    }
  }

  return ep == "-" ||
      (ep.size() == 2 && ep[0] >= 'a' && ep[0] <= 'h' &&
       ep[1] >= '1' && ep[1] <= '8');
}

bool parse_move(Game& game, const std::string& notation, Move* result) {
  if (notation.size() != 4 && notation.size() != 5) return false;

  MoveList legal_moves;
  const GenResult generated = gen_legal(game, legal_moves);
  for (u8 index = 0; index < generated.count; ++index) {
    char algebraic[6];
    legal_moves[index].store_alg(algebraic);
    if (notation == algebraic) {
      *result = legal_moves[index];
      return true;
    }
  }
  return false;
}

bool set_position(Game* game, const std::string& arguments) {
  std::istringstream moves(arguments);
  std::string marker;
  if (!(moves >> marker)) return false;

  Game candidate;
  if (marker == "startpos") {
    candidate = Game::initial();
  } else if (marker == "fen") {
    std::string fields[6];
    for (std::string& field : fields) {
      if (!(moves >> field)) return false;
    }
    const std::string fen = fields[0] + " " + fields[1] + " " + fields[2] +
        " " + fields[3] + " " + fields[4] + " " + fields[5];
    if (!is_valid_fen(fen)) return false;
    candidate = Game::from_fen(fen.c_str());
  } else {
    return false;
  }

  if (!(moves >> marker)) {
    *game = candidate;
    return true;
  }
  if (marker != "moves") return false;

  std::string notation;
  while (moves >> notation) {
    Move move;
    if (!parse_move(candidate, notation, &move)) return false;
    candidate.make_move(move);
  }

  *game = candidate;
  return true;
}

unsigned int parse_nonnegative(const std::string& value) {
  if (!is_decimal(value)) return 0;
  const unsigned long long parsed = std::strtoull(value.c_str(), nullptr, 10);
  return parsed > UINT_MAX ? UINT_MAX : static_cast<unsigned int>(parsed);
}

unsigned int time_for_go(const std::string& arguments, Color turn,
                         unsigned int overhead) {
  std::istringstream input(arguments);
  std::string key;
  unsigned int move_time = 0;
  unsigned int clock_time = 0;
  unsigned int increment = 0;
  unsigned int moves_to_go = 30;
  bool infinite = false;
  bool has_move_time = false;
  bool has_clock_time = false;

  while (input >> key) {
    if (key == "movetime" || key == "wtime" || key == "btime" ||
        key == "winc" || key == "binc" || key == "movestogo") {
      std::string value;
      if (!(input >> value)) break;
      const unsigned int millis = parse_nonnegative(value);
      if (key == "movestogo") moves_to_go = std::max(1u, millis);
      if (key == "movetime") {
        has_move_time = true;
        move_time = millis;
      }
      if ((key == "wtime" && turn == WHITE) ||
          (key == "btime" && turn == BLACK)) {
        has_clock_time = true;
        clock_time = millis;
      }
      if ((key == "winc" && turn == WHITE) ||
          (key == "binc" && turn == BLACK)) increment = millis;
    } else if (key == "infinite") infinite = true;
  }

  if (infinite) return UINT_MAX;
  if (has_move_time) return move_time > overhead ? move_time - overhead : 0;
  if (has_clock_time) {
    // The increment is credited after the move; never spend it in advance.
    const unsigned int available = clock_time > overhead ? clock_time - overhead : 0;
    const unsigned long long allocated =
        static_cast<unsigned long long>(available) / moves_to_go +
        static_cast<unsigned long long>(increment) * 3 / 4;
    return static_cast<unsigned int>(std::min<unsigned long long>(available, allocated));
  }
  return DEFAULT_MOVE_TIME_MS;
}

Move go(Game& game, unsigned int requested_time, Searcher& searcher) {
  MoveList legal_moves;
  const GenResult generated = gen_legal(game, legal_moves);
  if (generated.count == 0) {
    return Move::null();
  }

  searcher.set_search_time(std::min(MIN_SEARCH_TIME_MS, requested_time));
  Move best_move = searcher.get_best_move_parallel(game);

  return best_move;
}

void set_option(const std::string& arguments, Searcher& searcher,
                unsigned int& overhead) {
  std::istringstream input(arguments);
  std::string token, name, value, extra;
  if (!(input >> token) || token != "name") return;
  while (input >> token && token != "value") {
    if (!name.empty()) name += ' ';
    name += token;
  }
  if (token != "value" || !(input >> value) || (input >> extra) ||
      !is_decimal(value)) return;
  std::transform(name.begin(), name.end(), name.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  const unsigned int number = parse_nonnegative(value);
  if (name == "move overhead" && number <= MAX_MOVE_OVERHEAD_MS) {
    overhead = number;
  } else if (name == "threads" && number >= 1 && number <= 128) {
    searcher.thread_count = number;
  } else if (name == "hash" && number >= 1 && number <= 4096) {
    try {
      searcher.trans_table->resize(number);
    } catch (const std::bad_alloc&) {
      send("info string Unable to allocate requested Hash size; keeping previous table");
    }
  }
}

}  // namespace

namespace UCI {

void loop() {
  Game game = Game::initial();
  Searcher searcher(DEFAULT_MOVE_TIME_MS);
  unsigned int overhead = DEFAULT_MOVE_OVERHEAD_MS;
  std::thread worker;
  std::mutex stop_mutex;
  std::condition_variable stopped;
  auto stop = [&] {
    {
      std::lock_guard<std::mutex> lock(stop_mutex);
      searcher.cancel->store(true, std::memory_order_relaxed);
    }
    stopped.notify_all();
    if (worker.joinable()) worker.join();
  };
  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream input(line);
    std::string command;
    input >> command;
    std::string arguments;
    std::getline(input, arguments);

    if (command == "uci") {
      send("id name CrapBot\n"
           "id author Spoonvin\n"
           "option name Move Overhead type spin default 100 min 0 max 5000\n"
           "option name Threads type spin default 4 min 1 max 128\n"
           "option name Hash type spin default " +
           std::to_string(TT_SIZE * sizeof(TTSlot) / (1024 * 1024)) +
           " min 1 max 4096\nuciok");
    } else if (command == "isready") {
      send("readyok");
    } else if (command == "setoption") {
      stop();
      set_option(arguments, searcher, overhead);
    } else if (command == "ucinewgame") {
      stop();
      game = Game::initial();
      searcher.trans_table->init();
      searcher.trans_table->age = 0;
      for (auto& ply_killers : searcher.killers)
        std::fill(std::begin(ply_killers), std::end(ply_killers), Move::null());
    } else if (command == "position") {
      stop();
      set_position(&game, arguments);
    } else if (command == "go") {
      stop();
      searcher.cancel->store(false, std::memory_order_relaxed);
      const unsigned int duration = time_for_go(arguments, game.turn, overhead);
      std::istringstream parameters(arguments);
      std::string parameter;
      bool infinite = false;
      while (parameters >> parameter) infinite |= parameter == "infinite";
      worker = std::thread([&, position = game, duration, infinite]() mutable {
        const Move best_move = go(position, duration, searcher);
        if (infinite) {
          std::unique_lock<std::mutex> lock(stop_mutex);
          stopped.wait(lock, [&] { return searcher.cancel->load(std::memory_order_relaxed); });
        }
        char algebraic[6];
        if (best_move.is_null()) {
          send("bestmove 0000");
        } else {
          best_move.store_alg(algebraic);
          send(std::string("bestmove ") + algebraic);
        }
      });
    } else if (command == "stop") {
      stop();
    } else if (command == "quit") {
      break;
    }
  }
  stop();
}

}  // namespace UCI
