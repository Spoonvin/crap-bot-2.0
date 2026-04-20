#include <cstdlib>

#include "chess/game.h"
#include "chess/player.h"

Game Game::from_fen(const char* fen) {
  Game game{};
  game._from_fen(fen);
  return game;
}

void Game::_from_fen(const char* fen) {
  // Index of currently parsed character
  size_t i = 0;
  char c;

  // Piece placements
  for (i8 row = 7; row >= 0; row--) {
    i8 col = 0;
    while (col < 8) {
      c = fen[i++];  // Consume

      if (c == '/') {
        continue;
      } else if (isdigit(c)) {
        i8 di = c - '0';

        // Fill empty squares
        for (i8 j = 0; j < di; j++) {
          Pos pos = row * 8 + (col++);
          board[pos] = EMPTY_SQUARE;
        }
      } else {
        Square square = chrts(c);
        Pos pos = row * 8 + (col++);
        board[pos] = square;
      }
    }
  }

  // Consume space
  c = fen[i++];

  assert(c == ' ');

  // Side to move
  c = fen[i++];  // Consume
  switch (c) {
    case 'w': {
      turn = WHITE;
      break;
    }
    case 'b': {
      turn = BLACK;
      break;
    }
    default: {
      abort();
    }
  }

  // Consume space
  c = fen[i++];

  assert(c == ' ');

  // Castling ability
  players[WHITE].cstl_flags = KING_MOVED | LEFT_ROOK_MOVED | RIGHT_ROOK_MOVED;
  players[BLACK].cstl_flags = KING_MOVED | LEFT_ROOK_MOVED | RIGHT_ROOK_MOVED;

  c = fen[i++];  // Consume

  // Neither can castle
  if (c == '-') {
    // Consume space
    c = fen[i++];
  } else {
    do {
      Square square = chrts(c);
      Piece piece = stp(square);
      Color color = stc(square);
      switch (piece) {
        case QUEEN: {
          players[color].cstl_flags &= ~KING_MOVED;
          players[color].cstl_flags &= ~LEFT_ROOK_MOVED;

          break;
        }
        case KING: {
          players[color].cstl_flags &= ~KING_MOVED;
          players[color].cstl_flags &= ~RIGHT_ROOK_MOVED;

          break;
        }
        default: {
          abort();
        }
      }

      c = fen[i++];  // Consume
    } while (c != ' ');
  }

  assert(c == ' ');

  // En passant target square
  c = fen[i++];  // Consume

  // En passant not possible
  if (c == '-') {
    ep_pos = -1;
  } else {
    i8 col = c - 'a';
    i8 row = fen[i++] - '1';  // Consume
    ep_pos = row * 8 + col;
  }

  // Consume space
  c = fen[i++];

  assert(c == ' ');

  // Halfmove clock
  u8 hm_clock = 0;
  c = fen[i++];  // Consume
  do {
    hm_clock *= 10;
    hm_clock += c - '0';
    c = fen[i++];  // Consume
  } while (c != ' ');

  this->hm_clock = hm_clock;

  assert(c == ' ');

  // Fullmove counter
  u8 fm_counter = 0;
  c = fen[i++];  // Consume
  do {
    fm_counter *= 10;
    fm_counter += c - '0';
    c = fen[i++];  // Consume
  } while (c != '\0');

  this->fm_counter = fm_counter;

  assert(c == '\0');

  // Store bitboards
  board.store_bitboards(players[WHITE].bb, players[BLACK].bb);
}

void Game::store_fen(char buffer[MAX_FEN]) {
  char* ptr = buffer;

  // Piece placement
  for (i8 row = 7; row >= 0; row--) {
    int empty_count = 0;
    for (i8 col = 0; col < 8; col++) {
      Square square = board[row * 8 + col];
      if (square & EMPTY_SQUARE) {
        empty_count++;
      } else {
        if (empty_count > 0) {
          ptr += sprintf(ptr, "%d", empty_count);
          empty_count = 0;
        }
        *ptr++ = stchr(square);
      }
    }
    if (empty_count > 0) ptr += sprintf(ptr, "%d", empty_count);

    // Add delimiter
    *ptr++ = (row > 0) ? '/' : ' ';
  }

  // Side to move
  *ptr++ = (turn == WHITE) ? 'w' : 'b';
  *ptr++ = ' ';

  // Castling availability
  bool white_kingside =
    (!players[WHITE].king_moved() && !players[WHITE].rrook_moved());
  bool white_queenside =
    (!players[WHITE].king_moved() && !players[WHITE].lrook_moved());
  bool black_kingside =
    (!players[BLACK].king_moved() && !players[BLACK].rrook_moved());
  bool black_queenside =
    (!players[BLACK].king_moved() && !players[BLACK].lrook_moved());
  if (
    !white_kingside && !white_queenside && !black_kingside &&
    !black_queenside) {
    *ptr++ = '-';
  } else {
    if (white_kingside) *ptr++ = 'K';
    if (white_queenside) *ptr++ = 'Q';
    if (black_kingside) *ptr++ = 'k';
    if (black_queenside) *ptr++ = 'q';
  }
  *ptr++ = ' ';

  // En passant target square
  if (ep_pos == -1) {
    *ptr++ = '-';
  } else {
    i8 ep_row = ep_pos / 8;
    i8 ep_col = ep_pos % 8;

    char file = 'a' + ep_col;
    char rank = '1' + ep_row;

    *ptr++ = file;
    *ptr++ = rank;
  }
  *ptr++ = ' ';

  // Halfmove clock
  ptr += sprintf(ptr, "%d", hm_clock);
  *ptr++ = ' ';

  // Fullmove counter
  ptr += sprintf(ptr, "%d", fm_counter);
  *ptr = '\0';
}
