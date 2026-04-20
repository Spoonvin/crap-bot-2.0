#include "chess/player.h"

Player Player::initial() { return Player{.cstl_flags = 0, .bb = Bitboard{0}}; }

bool Player::lrook_moved() { return cstl_flags & LEFT_ROOK_MOVED; }

bool Player::rrook_moved() { return cstl_flags & RIGHT_ROOK_MOVED; }

bool Player::king_moved() { return cstl_flags & KING_MOVED; }

void Player::set_lrook_moved() { cstl_flags |= LEFT_ROOK_MOVED; }

void Player::set_rrook_moved() { cstl_flags |= RIGHT_ROOK_MOVED; }

void Player::set_king_moved() { cstl_flags |= KING_MOVED; }
