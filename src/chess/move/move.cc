#include "chess/move/move.h"

#include <cassert>
#include <csignal>

Move Move::normal(Pos from, Pos to) {
  u16 data = from | (to << 6);
  return Move{data};
}

Move Move::ep(Pos from, Pos to) {
  u16 data = from | (to << 6) | EN_PASSANT << 12;
  return Move{data};
}

Move Move::cstl(Pos from, Pos to) {
  u16 data = from | (to << 6) | CASTLING << 12;
  return Move{data};
}

Move Move::promo(Pos from, Pos to, Piece piece) {
  u16 data = from | (to << 6) | PROMOTION << 12 | (piece - ROOK) << 14;
  return Move{data};
}

Move Move::null() { return Move{0}; }

Pos Move::from() const { return data & 0b111111; }

Pos Move::to() const { return (data >> 6) & 0b111111; }

MoveType Move::type() const {
  return static_cast<MoveType>((data >> 12) & 0b11);
}

Piece Move::promo_piece() const {
  return static_cast<Piece>((data >> 14) + ROOK);
}

bool Move::is_null() const { return data == 0; }

void Move::invert() { data ^= 0b111000111000; }

void Move::store_alg(char buffer[6]) const {
  buffer[0] = 'a' + (this->from() & 7);
  buffer[1] = '1' + (this->from() >> 3);
  buffer[2] = 'a' + (this->to() & 7);
  buffer[3] = '1' + (this->to() >> 3);
  if (this->type() == PROMOTION) {
    switch (this->promo_piece()) {
      case QUEEN: {
        buffer[4] = 'q';
        break;
      }
      case ROOK: {
        buffer[4] = 'r';
        break;
      }
      case BISHOP: {
        buffer[4] = 'b';
        break;
      }
      case KNIGHT: {
        buffer[4] = 'n';
        break;
      }
      default: {
        raise(SIGABRT);
      }
    }
    buffer[5] = '\0';
  } else {
    buffer[4] = '\0';
  }
}
