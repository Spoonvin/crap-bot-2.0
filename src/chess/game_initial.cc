#include "chess/game.h"
#include "search/zobrist_hash.h"

Game Game::initial() {
  Game game{};
  game._initial();
  game.hash = gen_zob_hash(game);
  return game;
}

void Game::_initial() {
  turn = WHITE;

  // Initialize board and players
  board = Board::initial();
  players[WHITE] = Player::initial();
  players[BLACK] = Player::initial();

  // Store bitboard representations
  board.store_bitboards(players[WHITE].bb, players[BLACK].bb);

  // No dp
  ep_pos = -1;

  hm_clock = 0;
  fm_counter = 1;
}
