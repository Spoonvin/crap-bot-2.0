#include "UCI/uci.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

#include "chess/game.h"
#include "chess/move/movegen.h"
#include "search/search.h"

namespace {

constexpr unsigned int DEFAULT_MOVE_TIME_MS = 800;
constexpr unsigned int MIN_SEARCH_TIME_MS = 50;

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

unsigned int time_for_go(const std::string& arguments, Color turn) {
  std::istringstream input(arguments);
  std::string key;
  unsigned int move_time = 0;
  unsigned int clock_time = 0;
  unsigned int increment = 0;
  bool has_move_time = false;
  bool has_clock_time = false;

  while (input >> key) {
    if (key == "movetime" || key == "wtime" || key == "btime" ||
        key == "winc" || key == "binc") {
      std::string value;
      if (!(input >> value)) break;
      const unsigned int millis = parse_nonnegative(value);
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
    }
  }

  if (has_move_time) return move_time;
  if (has_clock_time) {
    // Keep most of the clock in reserve; this engine has no pondering or
    // asynchronous stop support yet.
    const unsigned long long allocated =
        static_cast<unsigned long long>(clock_time) / 30 +
        static_cast<unsigned long long>(increment) * 3 / 4;
    return std::max(1u, allocated > UINT_MAX
        ? UINT_MAX
        : static_cast<unsigned int>(allocated));
  }
  return DEFAULT_MOVE_TIME_MS;
}

void go(Game& game, const std::string& arguments, Searcher& searcher) {
  MoveList legal_moves;
  const GenResult generated = gen_legal(game, legal_moves);
  if (generated.count == 0) {
    std::cout << "bestmove 0000\n" << std::flush;
    return;
  }

  const unsigned int requested_time = time_for_go(arguments, game.turn);
  Move best_move = legal_moves[0];
  if (requested_time >= MIN_SEARCH_TIME_MS) {
    searcher.set_search_time(requested_time);
    best_move = searcher.get_best_move(game);
  }

  char algebraic[6];
  best_move.store_alg(algebraic);
  std::cout << "bestmove " << algebraic << '\n' << std::flush;
}

}  // namespace

namespace UCI {

void loop() {
  Game game = Game::initial();
  Searcher searcher(DEFAULT_MOVE_TIME_MS);
  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream input(line);
    std::string command;
    input >> command;

    if (command == "uci") {
      std::cout << "id name Chess Parallel\n"
                << "id author Chess Parallel contributors\n"
                << "uciok\n" << std::flush;
    } else if (command == "isready") {
      std::cout << "readyok\n" << std::flush;
    } else if (command == "ucinewgame") {
      game = Game::initial();
    } else if (command == "position") {
      const size_t first_space = line.find_first_of(" \t");
      if (first_space != std::string::npos) {
        set_position(&game, line.substr(first_space));
      }
    } else if (command == "go") {
      const size_t first_space = line.find_first_of(" \t");
      go(game, first_space == std::string::npos ? "" : line.substr(first_space),
         searcher);
    } else if (command == "quit") {
      break;
    }
  }
}

}  // namespace UCI
