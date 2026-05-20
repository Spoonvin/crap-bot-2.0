#pragma once

#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <random>

#include "chess/game.h"

struct OpeningBook {
private:
    std::shared_ptr<
    std::unordered_map<u64, std::vector<std::string>>> posMoves;
    std::mt19937 rng;

    Move book_move_to_move(const std::string& bookMove, Game& game);

public:
    OpeningBook(const std::string& file);

    // Looks up opening moves and returns a random one
    Move lookup_position(Game& game);
};