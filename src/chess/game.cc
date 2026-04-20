#include "chess/game.h"

#include "chess/board/mask_operations.h"
#include "chess/move/piece/king.h"

void Game::make_move(Move move) {
  Player& act = players[turn];
  Player& wait = players[!turn];

  MoveType move_type = move.type();

  // Moved piece
  Piece piece = stp(board[move.from()]);

  Pos from = move.from();
  Pos to = move.to();

  switch (move_type) {
    case NORMAL: {
      make_normal(act, wait, piece, from, to);
      break;
    }
    case PROMOTION: {
      Piece promo_piece = move.promo_piece();
      make_promo(act, wait, from, to, promo_piece);
      break;
    }
    case EN_PASSANT: {
      make_ep(act, wait, from, to);
      break;
    }
    case CASTLING: {
      make_cstl(act, from, to);
      break;
    }
  }

  // Reset en passant target square (this is set by pawn_effect)
  ep_pos = -1;

  // Special side-effects for some pieces

  switch (piece) {
    case PAWN: {
      pawn_effect(from, to);
      break;
    }
    case KING: {
      king_effect(act);
      break;
    }
    case ROOK: {
      rook_effect(act, from);
      break;
    }
    default: {
      break;
    }
  }

  // Check if any rooks were captured
  // Can happen on either player's turn (atomic patch)

  Mask white_q_rook = 0x1ull;
  Mask white_k_rook = 0x80;
  Mask black_q_rook = 0x100000000000000ull;
  Mask black_k_rook = 0x8000000000000000ull;

  if (!(white_q_rook & players[WHITE].bb[ROOK])) {
    players[WHITE].set_lrook_moved();
  }

  if (!(white_k_rook & players[WHITE].bb[ROOK])) {
    players[WHITE].set_rrook_moved();
  }

  if (!(black_q_rook & players[BLACK].bb[ROOK])) {
    players[BLACK].set_lrook_moved();
  }

  if (!(black_k_rook & players[BLACK].bb[ROOK])) {
    players[BLACK].set_rrook_moved();
  }

  next_turn();
}

u64 Game::hash() const {
  u64 hash = board.hash();

  hash ^= turn;
  hash ^= players[WHITE].cstl_flags << 1;
  hash ^= players[BLACK].cstl_flags << 9;

  if (ep_pos != -1) {
    hash ^= pos_mask(ep_pos);
  }

  return hash;
}

void Game::next_turn() {
  // Toggle active player
  turn = Color(!turn);

  // Increment fullmove counter
  // Should be one for the first round
  if (turn == WHITE) {
    fm_counter++;
  }

  hm_clock++;
}

// With known piece
void Game::move_piece(Player& player, Piece piece, Pos from, Pos to) {
  if (!(board[to] & EMPTY_SQUARE)) {
    // Remove piece at landing square
    Square square = board[to];
    Player& captured_player = players[stc(square)];
    captured_player.bb[stp(square)] ^= pos_mask(to);
  }

  // Update bitboard
  player.bb[piece] ^= pos_mask(from) | pos_mask(to);

  // Update board
  board[to] = board[from];
  board[from] = EMPTY_SQUARE;
}

// Move types

void Game::make_normal(
  Player& act,
  Player& wait,
  Piece piece,
  Pos from,
  Pos to) {
  bool is_capture = !(board[to] & EMPTY_SQUARE);

  // First move the piece as normal
  move_piece(act, piece, from, to);
}

void Game::make_promo(
  Player& act,
  Player& wait,
  Pos from,
  Pos to,
  Piece promo_piece) {
  bool is_capture = !(board[to] & EMPTY_SQUARE);

  if (is_capture) {
    // Remove piece at landing square
    Square square = board[to];
    Player& captured_player = players[stc(square)];
    captured_player.bb[stp(square)] ^= pos_mask(to);
  }

  // Update board
  board[from] = EMPTY_SQUARE;
  board[to] = pcts(promo_piece, turn);

  // Update bitboards
  act.bb[PAWN] ^= pos_mask(from);
  act.bb[promo_piece] ^= pos_mask(to);
}

void Game::make_ep(Player& act, Player& wait, Pos from, Pos to) {
  Pos dp_pos = (from & ~7) |  // Row of from-postion
               (ep_pos & 7);  // Column of en-passant-position

  // Capture double pushed pawn
  board[dp_pos] = EMPTY_SQUARE;
  wait.bb[PAWN] ^= pos_mask(dp_pos);

  // Move pawn
  move_piece(act, PAWN, from, to);
}

void Game::make_cstl(Player& act, Pos from, Pos to) {
  // Extract column of target square
  i8 to_col = to & 7;

  Pos rook_from, rook_to;

  switch (to_col) {
    case 2: {
      // Castling to left
      rook_to = from - 1;
      rook_from = from - 4;
      break;
    }

    case 6: {
      // Castling to right
      rook_to = from + 1;
      rook_from = from + 3;
      break;
    }
    default: {
      // Shouldn't happen
      abort();
    }
  }

  // Move rook
  move_piece(act, ROOK, rook_from, rook_to);

  // Move king
  move_piece(act, KING, from, to);
}

// Special side-effects

void Game::pawn_effect(Pos from, Pos to) {
  // Reset halfmove clock
  hm_clock = 0;

  // If double push, update target en passant square
  if ((from ^ to) == 16) {
    ep_pos = (from + to) >> 1;
  }
}

void Game::king_effect(Player& act) {
  // Update castling ability
  act.set_king_moved();
}

void Game::rook_effect(Player& act, Pos from) {
  // Normalize rook position
  Pos norm_from = turn == BLACK ? from - 56 : from;

  // Update castling ability
  switch (norm_from) {
    case 0: {
      act.set_lrook_moved();
      break;
    }
    case 7: {
      act.set_rrook_moved();
      break;
    }
  }
}

bool Game::is_draw() { return is_insuff() || is_50mr() || is_3fr(); }

bool Game::is_3fr() {
  // Check if current position has been repeated 3 times
  return false;
}

bool Game::is_insuff() {
  // Possible conditions:
  // - King vs king
  // - King vs king and bishop
  // - King vs king and knight
  // - King and bishop vs king and bishop on same color

  Player& act = players[turn];
  Player& wait = players[!turn];

  // Count pieces

  u8 act_count = __builtin_popcountll(act.bb.occupancy());
  u8 wait_count = __builtin_popcountll(wait.bb.occupancy());

  // If either player has more than two pieces, it is not a draw by
  // insufficient material
  if (act_count > 2 || wait_count > 2) {
    return false;
  }

  switch (act_count) {
    case 1: {
      // Acting player has one remaining piece (king)

      switch (wait_count) {
        case 1: {
          // Waiting player has one remaining piece (king)
          return true;
        }
        case 2: {
          // Waiting player has two remaining pieces
          // If the other piece is a bishop or knight, it is a draw
          if ((wait.bb[BISHOP] | wait.bb[KNIGHT]) != 0ull) {
            return true;
          }

          return false;
        }
        default: {
          abort();
        }
      }

      break;
    }
    case 2: {
      // Acting player has two remaining pieces

      Mask act_bishops = act.bb[BISHOP];

      if (act_bishops == 0) {
        // If acting player has no bishops, it is not a draw
        return false;
      }

      Mask wait_bishops = act.bb[BISHOP];

      if (wait_bishops == 0) {
        // If waiting player has no bishops, it is not a draw
        return false;
      }

      Mask bishops = act_bishops | wait_bishops;

      if ((bishops & BLACK_SQUARES) || (bishops & WHITE_SQUARES)) {
        // Both players have bishops on same color, insufficient material
        return true;
      }

      return false;

      break;
    }
    default: {
      abort();
    }
  }
}

bool Game::is_50mr() {
  // Each player must make 100 moves without a capture or pawn move
  return hm_clock >= 100;
}

// Return true if either king is dead
bool Game::is_king_dead() {
  // Both kings cannot be dead at the same time
  assert(players[WHITE].bb[KING] || players[BLACK].bb[KING]);

  return !players[WHITE].bb[KING] || !players[BLACK].bb[KING];
}

void Game::invert() {
  turn = Color(!turn);

  // Invert the board
  board.invert();

  // Invert row of ep pos
  if (ep_pos != -1) {
    ep_pos ^= 56;
  }

  // Swap and invert bitboards

  Player& white_player = players[WHITE];
  Player& black_player = players[BLACK];

  for (i8 i = 0; i < PIECE_COUNT; i++) {
    Piece piece = Piece(i);

    Mask white_mask = white_player.bb[piece];
    Mask black_mask = black_player.bb[piece];

    // Invert order of bytes
    white_mask = __builtin_bswap64(white_mask);
    black_mask = __builtin_bswap64(black_mask);

    white_player.bb[piece] = black_mask;
    black_player.bb[piece] = white_mask;
  }

  // Swap castling rights

  i8 white_cstl_flags = players[WHITE].cstl_flags;

  players[WHITE].cstl_flags = players[BLACK].cstl_flags;
  players[BLACK].cstl_flags = white_cstl_flags;
}
