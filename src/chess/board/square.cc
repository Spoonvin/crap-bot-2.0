#include "chess/board/square.h"

Piece stp(Square square) { return Piece(square & 7); }

Color stc(Square square) { return Color(square >> 3); }

Square pcts(Piece piece, Color color) { return Square(piece | (color << 3)); }

Square chrts(char c) {
  switch (c) {
    case 'P': {
      return WHITE_PAWN;
    }
    case 'R': {
      return WHITE_ROOK;
    }
    case 'N': {
      return WHITE_KNIGHT;
    }
    case 'B': {
      return WHITE_BISHOP;
    }
    case 'Q': {
      return WHITE_QUEEN;
    }
    case 'K': {
      return WHITE_KING;
    }
    case 'p': {
      return BLACK_PAWN;
    }
    case 'r': {
      return BLACK_ROOK;
    }
    case 'n': {
      return BLACK_KNIGHT;
    }
    case 'b': {
      return BLACK_BISHOP;
    }
    case 'q': {
      return BLACK_QUEEN;
    }
    case 'k': {
      return BLACK_KING;
    }
    default: {
      abort();
    }
  }
}

char stchr(Square square) {
  switch (square) {
    case WHITE_PAWN: {
      return 'P';
    }
    case WHITE_ROOK: {
      return 'R';
    }
    case WHITE_KNIGHT: {
      return 'N';
    }
    case WHITE_BISHOP: {
      return 'B';
    }
    case WHITE_QUEEN: {
      return 'Q';
    }
    case WHITE_KING: {
      return 'K';
    }
    case BLACK_PAWN: {
      return 'p';
    }
    case BLACK_ROOK: {
      return 'r';
    }
    case BLACK_KNIGHT: {
      return 'n';
    }
    case BLACK_BISHOP: {
      return 'b';
    }
    case BLACK_QUEEN: {
      return 'q';
    }
    case BLACK_KING: {
      return 'k';
    }
    case EMPTY_SQUARE: {
      abort();
    }
    default: {
      abort();
    }
  }
}
