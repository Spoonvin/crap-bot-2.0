#include "chess/board/board.h"

#include <cstring>

#include "chess/board/mask_operations.h"

const BoardRow ROW_INVERSION_MASK = 0x0808080808080808ull;
u64 ROW_RANDS[8];

__attribute__((constructor))

void
init_row_rands() {
  for (i8 i = 0; i < 8; i++) {
    // Random bitstring
    u32 top = rand();
    u32 bottom = rand();
    ROW_RANDS[i] = (u64(top) << 32) | bottom;
  }
}

Square& Board::operator[](Pos pos) {
  return reinterpret_cast<Square*>(rows)[pos];
}

Board Board::initial() {
  Board board;

  board[0] = WHITE_ROOK;
  board[1] = WHITE_KNIGHT;
  board[2] = WHITE_BISHOP;
  board[3] = WHITE_QUEEN;
  board[4] = WHITE_KING;
  board[5] = WHITE_BISHOP;
  board[6] = WHITE_KNIGHT;
  board[7] = WHITE_ROOK;

  for (Pos pos = 8; pos < 16; pos++) {
    board[pos] = WHITE_PAWN;
  }

  for (Pos pos = 16; pos < 48; pos++) {
    board[pos] = EMPTY_SQUARE;
  }

  for (Pos pos = 48; pos < 56; pos++) {
    board[pos] = BLACK_PAWN;
  }

  board[56] = BLACK_ROOK;
  board[57] = BLACK_KNIGHT;
  board[58] = BLACK_BISHOP;
  board[59] = BLACK_QUEEN;
  board[60] = BLACK_KING;
  board[61] = BLACK_BISHOP;
  board[62] = BLACK_KNIGHT;
  board[63] = BLACK_ROOK;

  return board;
}

void Board::invert() {
  for (i8 row = 0; row < 4; row++) {
    // Swap rows
    BoardRow temp = rows[row];
    rows[row] = rows[7 - row];
    rows[7 - row] = temp;

    // Invert color of squares
    rows[row] ^= ROW_INVERSION_MASK;
    rows[7 - row] ^= ROW_INVERSION_MASK;
  }
}

void Board::store_bitboards(Bitboard& white_buffer, Bitboard& black_buffer) {
  memset(&white_buffer, 0, sizeof(white_buffer));
  memset(&black_buffer, 0, sizeof(black_buffer));

  for (Pos pos = 0; pos < 64; pos++) {
    Square square = this->operator[](pos);

    // Ignore empty squares
    if (square & EMPTY_SQUARE) {
      continue;
    }

    Piece piece = stp(square);
    Color color = stc(square);

    Mask mask = pos_mask(pos);

    switch (color) {
      case WHITE: {
        white_buffer[piece] |= mask;
        break;
      }
      case BLACK: {
        black_buffer[piece] |= mask;
        break;
      }
    }
  }
}

u64 Board::hash() const {
  u64 hash = 0;

  // XOR each row with a random number, alternating shifts
  for (i8 i = 0; i < 8; i++) {
    BoardRow row = rows[i];
    u8 shift = (i % 2) << 2;
    hash ^= (row << shift) ^ ROW_RANDS[i];
  }

  return hash;
}
