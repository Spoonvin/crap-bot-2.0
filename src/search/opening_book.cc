#include <unordered_map>
#include <vector>
#include <string>
#include <sstream>
#include <random>
#include <algorithm>
#include <fstream>

#include "search/opening_book.h"
#include "chess/move/movegen.h"
#include <iostream>

OpeningBook::OpeningBook(const std::string& path) {
    std::random_device rd;
    rng = std::mt19937(rd());

    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Failed to open book file");
    }

    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string file = buffer.str();

    std::stringstream ss(file);
    std::string segment;

    // Split on "pos "
    std::vector<std::string> entries;
    size_t start = 0;
    size_t pos;

    while ((pos = file.find("pos ", start)) != std::string::npos) {
        if (pos != start) {
            entries.push_back(file.substr(start, pos - start));
        }
        start = pos + 4;
    }
    entries.push_back(file.substr(start));

    for (const std::string& entry : entries) {
        if (entry.empty()) continue;

        std::stringstream lineStream(entry);
        std::string line;
        std::vector<std::string> lines;

        while (std::getline(lineStream, line)) {
            if (!line.empty())
                lines.push_back(line);
        }

        if (lines.empty()) continue;

        std::string fen = lines[0];
        u64 fen_hash = Game::from_fen(fen.c_str()).hash;

        std::vector<std::string> moves;
        for (size_t i = 1; i < lines.size(); i++) {
            moves.push_back(lines[i]);
        }

        posMoves[fen_hash] = moves;
    }
}

// Looks up opening moves and returns a random one
Move OpeningBook::lookup_position(Game& game) {
    u64 game_hash = game.hash;

    auto it = posMoves.find(game_hash);
    if (it != posMoves.end()) {
        const std::vector<std::string>& moves = it->second;

        std::uniform_int_distribution<int> dist(0, moves.size() - 1);
        int index = dist(rng);

        std::string moveLine = moves[index];

        // Take first token (before space)
        std::string bookMove = moveLine.substr(0, moveLine.find(' '));

        return book_move_to_move(bookMove, game);
    }

    return Move::null();
}

// Convert string like "e2e4" into Move
Move OpeningBook::book_move_to_move(const std::string& bookMove, Game& game) {
    const int a_ascii = 97;

    char srcCol = bookMove[0];
    char srcRow = bookMove[1];
    char dstCol = bookMove[2];
    char dstRow = bookMove[3];

    Pos src = (srcRow - '1') * 8 + (srcCol - 'a');
    Pos dst = (dstRow - '1') * 8 + (dstCol - 'a');

    MoveList moves;
    GenResult gen_result = gen_legal(game, moves);

    for (Move m : moves) {
        if (m.from() == src && m.to() == dst)
            return m;
    }

    return Move::null();
}