#include "chess/move/movegen.h"

#include <csignal>
#include <cstdio>
#include <cstdlib>

#include "chess/board/mask_operations.h"
#include "chess/move/piece/bishop.h"
#include "chess/move/piece/black_pawn.h"
#include "chess/move/piece/king.h"
#include "chess/move/piece/knight.h"
#include "chess/move/piece/rook.h"
#include "chess/move/piece/white_pawn.h"

Mask pawn_atk_mask(Color turn, Pos pos) {
  switch (turn) {
    case WHITE: {
      return white_pawn_atk_mask(pos);
    }
    case BLACK: {
      return black_pawn_atk_mask(pos);
    }
  }
}

u8 insert_moves(Pos from, Mask to_mask, MoveList& moves, u8 count) {
  while (to_mask) {
    Pos to = pop_pos(to_mask);
    moves[count++] = Move::normal(from, to);
  }

  return count;
}

u8 insert_promos(Pos from, Pos to, MoveList& moves, u8 count) {
  moves[count++] = Move::promo(from, to, KNIGHT);
  moves[count++] = Move::promo(from, to, QUEEN);
  moves[count++] = Move::promo(from, to, ROOK);
  moves[count++] = Move::promo(from, to, BISHOP);

  return count;
}

struct MoveGenerator {
  Color turn;
  Player &act, &wait;

  Mask act_occ, wait_cc;
  Mask occ;

  Pos king_pos;

  Pos ep_pos;

  bool non_quiet_generation;

  MoveGenerator(Game& game, bool non_quiet)
      : turn(game.turn),
        act(game.players[turn]),
        wait(game.players[!turn]),
        ep_pos(game.ep_pos),
        non_quiet_generation(non_quiet) {
    act_occ = act.bb.occupancy();
    wait_cc = wait.bb.occupancy();
    occ = act_occ | wait_cc;

    assert(act.bb[KING] != 0ull);

    king_pos = __builtin_ctzll(act.bb[KING]);
  }

  // Finds all checks
  Check find_checks(Mask& restrict) {
    Check ct = NO_CHECK;
    u8 check_count = 0;
    restrict = 0ull;

    Mask pawn_atk = pawn_atk_mask(turn, king_pos);

    Mask pawn_checker = pawn_atk & wait.bb[PAWN];

    if (pawn_checker) {
      restrict = pawn_checker;
      ct = DIRECT_CHECK;
      check_count++;
    }

    Mask knight_atk = knight_atk_mask(king_pos);

    Mask knight_checker = knight_atk & wait.bb[KNIGHT];

    if (knight_checker) {
      restrict = knight_checker;
      ct = DIRECT_CHECK;
      check_count++;
    }

    Mask rook_atk = rook_atk_mask(occ, king_pos);

    Mask rook_checkers = rook_atk & wait.bb[ROOK];

    if (rook_checkers) {
      if (__builtin_popcountll(rook_checkers) > 1) {
        return DOUBLE_CHECK;
      }

      Mask ray =
        between_mask(__builtin_ctzll(rook_checkers), king_pos) | rook_checkers;

      restrict = ray;

      ct = SLIDE_CHECK;
      check_count++;
    }

    Mask bishop_atk = bishop_atk_mask(occ, king_pos);

    Mask bishop_checkers = bishop_atk & wait.bb[BISHOP];

    if (bishop_checkers) {
      if (__builtin_popcountll(bishop_checkers) > 1) {
        return DOUBLE_CHECK;
      }

      Mask ray = between_mask(__builtin_ctzll(bishop_checkers), king_pos) |
                 bishop_checkers;

      restrict = ray;

      ct = SLIDE_CHECK;
      check_count++;
    }

    Mask queen_atk = rook_atk | bishop_atk;

    Mask queen_checkers = queen_atk & wait.bb[QUEEN];

    if (queen_checkers) {
      if (__builtin_popcountll(queen_checkers) > 1) {
        return DOUBLE_CHECK;
      }

      Mask ray = between_mask(__builtin_ctzll(queen_checkers), king_pos) |
                 queen_checkers;

      restrict = ray;

      ct = SLIDE_CHECK;
      check_count++;
    }

    // If more than one checker
    if (check_count > 1) {
      return DOUBLE_CHECK;
    }

    return ct;
  }

  u8 gen_pawn_atks(Mask restrict, MoveList& moves, u8 count) {
    Mask pawns = act.bb[PAWN];
    Mask promo_pawns = pawns & (turn == WHITE ? ROW_6 : ROW_1);

    // Exclude promo-pawns from normal attacks
    pawns ^= promo_pawns;

    // Generate normal attacks

    while (pawns) {
      Pos pos = pop_pos(pawns);
      Mask atk = pawn_atk_mask(turn, pos) & wait_cc& restrict;
      count = insert_moves(pos, atk, moves, count);
    }

    // Generate promo-attacks

    while (promo_pawns) {
      Pos pos = pop_pos(promo_pawns);
      Mask atk = pawn_atk_mask(turn, pos) & wait_cc& restrict;

      while (atk) {
        Pos to = pop_pos(atk);
        count = insert_promos(pos, to, moves, count);
      }
    }

    return count;
  }

  u8 gen_wpawn_pshs(Mask restrict, MoveList& moves, u8 count) {
    Mask pawns = act.bb[PAWN];
    Mask promo_pawns = pawns & ROW_6;

    // Exclude promo-pawns from normal pushes
    pawns ^= promo_pawns;

    // Generate single-push moves

    Mask single = shift_mask_up(pawns, 1) & ~occ& restrict;

    while (single) {
      Pos to = pop_pos(single);
      moves[count++] = Move::normal(to - 8, to);
    }

    // Generate double-push moves

    Mask first = shift_mask_up(pawns & ROW_1, 1) & ~occ;
    Mask second = shift_mask_up(first, 1) & ~occ& restrict;

    while (second) {
      Pos to = pop_pos(second);
      moves[count++] = Move::normal(to - 16, to);
    }

    // Generate promo-push moves

    promo_pawns = shift_mask_up(promo_pawns, 1) & ~occ& restrict;

    while (promo_pawns) {
      Pos to = pop_pos(promo_pawns);
      count = insert_promos(to - 8, to, moves, count);
    }

    return count;
  }

  u8 gen_bpawn_pshs(Mask restrict, MoveList& moves, u8 count) {
    Mask pawns = act.bb[PAWN];
    Mask promo_pawns = pawns & ROW_1;

    // Exclude promo-pawns from normal pushes
    pawns ^= promo_pawns;

    // Generate single-push moves

    Mask single = shift_mask_down(pawns, 1) & ~occ& restrict;

    while (single) {
      Pos to = pop_pos(single);
      moves[count++] = Move::normal(to + 8, to);
    }

    // Generate double-push moves

    Mask first = shift_mask_down(pawns & ROW_6, 1) & ~occ;
    Mask second = shift_mask_down(first, 1) & ~occ& restrict;

    while (second) {
      Pos to = pop_pos(second);
      moves[count++] = Move::normal(to + 16, to);
    }

    // Generate promo-push moves

    promo_pawns = shift_mask_down(promo_pawns, 1) & ~occ& restrict;

    while (promo_pawns) {
      Pos to = pop_pos(promo_pawns);
      count = insert_promos(to + 8, to, moves, count);
    }

    return count;
  }

  u8 gen_pawn_pshs(Mask restrict, MoveList& moves, u8 count) {
    switch (turn) {
      case WHITE: {
        return gen_wpawn_pshs(restrict, moves, count);
      }
      case BLACK: {
        return gen_bpawn_pshs(restrict, moves, count);
      }
    }
  }

  u8 gen_pawn_ep(Mask restrict, MoveList& moves, u8 count) {
    if (ep_pos == -1) {
      return count;
    };

    Pos dp_pos = ep_pos + (turn == WHITE ? -8 : 8);

    Mask ep_mask = pos_mask(ep_pos);
    Mask dp_mask = pos_mask(dp_pos);

    // En passant move is pseudo-legal if dp pos is part of restriction
    // or, if ep pos is part of restrction
    // These are mutually exclusive when checked (true?)

    if (!((dp_mask | ep_mask)& restrict)) {
      return count;
    }

    // Find pawns adjacent to dp_pos, these are candidates

    Pos dp_col = dp_pos & 7;

    Mask pawns = act.bb[PAWN];
    Mask ep_pawns = 0ull;

    if (dp_col != 0) {
      // If not at left-most column, check left
      ep_pawns |= pos_mask(dp_pos - 1);
    }

    if (dp_col != 7) {
      // If not at left-most column, check right
      ep_pawns |= pos_mask(dp_pos + 1);
    }

    ep_pawns &= pawns;

    while (ep_pawns) {
      Pos pos = pop_pos(ep_pawns);

      // Verify that en passant does not expose king to check

      // Preview occupancy after en passant
      Mask occ_ep = occ ^ (dp_mask | pos_mask(pos));

      // Test king against sliders
      if (in_slide_atk(king_pos, occ_ep)) {
        continue;
      }

      moves[count++] = Move::ep(pos, ep_pos);
    }

    return count;
  }

  u8 gen_knight(Mask restrict, MoveList& moves, u8 count) {
    Mask knights = act.bb[KNIGHT];

    while (knights) {
      Pos pos = pop_pos(knights);
      Mask atk = knight_atk_mask(pos) & ~act_occ& restrict;
      count = insert_moves(pos, atk, moves, count);
    }

    return count;
  }

  u8 gen_bishop(Mask restrict, MoveList& moves, u8 count) {
    Mask bishops = act.bb[BISHOP];

    while (bishops) {
      Pos pos = pop_pos(bishops);
      Mask atk = bishop_atk_mask(occ, pos) & ~act_occ & restrict;
      count = insert_moves(pos, atk, moves, count);
    }

    return count;
  }

  u8 gen_rook(Mask restrict, MoveList& moves, u8 count) {
    Mask rooks = act.bb[ROOK];

    while (rooks) {
      Pos pos = pop_pos(rooks);
      Mask atk = rook_atk_mask(occ, pos) & ~act_occ& restrict;
      count = insert_moves(pos, atk, moves, count);
    }

    return count;
  }

  u8 gen_queen(Mask restrict, MoveList& moves, u8 count) {
    Mask queens = act.bb[QUEEN];

    while (queens) {
      Pos pos = pop_pos(queens);
      Mask atk = (rook_atk_mask(occ, pos) | bishop_atk_mask(occ, pos)) &
                 ~act_occ& restrict;
      count = insert_moves(pos, atk, moves, count);
    }

    return count;
  }

  u8 gen_atks(Mask restrict, MoveList& moves, u8 count) {
    count = gen_pawn_atks(restrict, moves, count);
    count = gen_pawn_ep(restrict, moves, count);
    count = gen_knight(restrict, moves, count);
    count = gen_bishop(restrict, moves, count);
    count = gen_rook(restrict, moves, count);
    count = gen_queen(restrict, moves, count);

    return count;
  }

  u8 gen_all(MoveList& moves) {
    u8 count = 0;

    if (!non_quiet_generation) count = gen_cstl(moves, count);
    if (!non_quiet_generation) count = gen_pawn_pshs(FULL_BOARD, moves, count);
    Mask restrict = (non_quiet_generation) ? wait_cc : FULL_BOARD; 
    count = gen_atks(restrict, moves, count);

    return count;
  }

  u8 gen_blocks(Mask restrict, MoveList& moves) {
    u8 count = 0;

    count = gen_pawn_pshs(restrict, moves, count);
    count = gen_atks(restrict, moves, count);

    return count;
  }

  u8 in_slide_atk(Pos pos, Mask occ) {
    Mask rook_atk = rook_atk_mask(occ, pos);

    if (rook_atk & wait.bb[ROOK]) {
      return true;
    }

    Mask bishop_atk = bishop_atk_mask(occ, pos);

    if (bishop_atk & wait.bb[BISHOP]) {
      return true;
    }

    Mask queen_atk = rook_atk | bishop_atk;

    if (queen_atk & wait.bb[QUEEN]) {
      return true;
    }

    return false;
  }

  u8 in_direct_atk(Pos pos) {
    Mask pawn_atk = pawn_atk_mask(turn, pos);

    if (pawn_atk & wait.bb[PAWN]) {
      return true;
    }

    Mask knight_atk = knight_atk_mask(pos);

    if (knight_atk & wait.bb[KNIGHT]) {
      return true;
    }

    Mask king_atk = king_atk_mask(pos);

    if (king_atk & wait.bb[KING]) {
      return true;
    }

    return false;
  }

  bool in_attack(Pos pos, Mask occ) {
    if (in_slide_atk(pos, occ)) {
      return true;
    }

    if (in_direct_atk(pos)) {
      return true;
    }

    return false;
  }

  u8 gen_king(MoveList& moves, u8 count, Check ct) {
    Mask atk = king_atk_mask(king_pos) & ~act_occ;
    if (non_quiet_generation && ct == NO_CHECK) atk &= wait_cc;

    // Exclude king from occupancy to prevent self-blocking attacks
    Mask occ = this->occ ^ pos_mask(king_pos);

    while (atk) {
      Pos to = pop_pos(atk);

      // Ensure move is not attacked

      if (in_attack(to, occ)) {
        continue;
      }

      moves[count++] = Move::normal(king_pos, to);
    }

    return count;
  }

  u8 gen_cstl(MoveList& moves, u8 count) {
    // Castling is only possible if king has not moved
    if (act.king_moved()) {
      return count;
    }

    Mask cstl_q;
    Mask cstl_k;

    switch (turn) {
      case WHITE: {
        cstl_q = WHITE_CSTL_Q;
        cstl_k = WHITE_CSTL_K;
        break;
      }
      case BLACK: {
        cstl_q = BLACK_CSTL_Q;
        cstl_k = BLACK_CSTL_K;
        break;
      }
    }

    if (!act.lrook_moved()) {
      if (
        !(occ & cstl_q) &&                // No pieces between king and rook
        !in_attack(king_pos - 1, occ) &&  // No attacks on king's path
        !in_attack(king_pos - 2, occ)) {
        moves[count++] = Move::cstl(king_pos, king_pos - 2);
      }
    }

    if (!act.rrook_moved()) {
      if (
        !(occ & cstl_k) &&                // No pieces between king and rook
        !in_attack(king_pos + 1, occ) &&  // No attacks on king's path
        !in_attack(king_pos + 2, occ)) {
        moves[count++] = Move::cstl(king_pos, king_pos + 2);
      }
    }

    return count;
  }

  u8 find_pins(Mask& pinned, Mask pin_rays[8]) {
    pinned = 0ull;
    u8 pinned_count = 0;

    // Compute pinned pieces

    Mask rook_atk = rook_atk_mask(wait_cc, king_pos);

    Mask rook_pinners = rook_atk & wait.bb[ROOK];

    while (rook_pinners) {
      Pos pinner = pop_pos(rook_pinners);
      Mask pinner_mask = pos_mask(pinner);

      // Compute rook ray as mask between pinner and king, including pinner

      Mask rook_ray = between_mask(pinner, king_pos) | pinner_mask;

      // If only one piece in ray, it is pinned

      if (__builtin_popcountll(rook_ray & act_occ) == 1) {
        pinned |= rook_ray & act_occ;
        pin_rays[pinned_count++] = rook_ray;
      }
    }

    Mask bishop_atk = bishop_atk_mask(wait_cc, king_pos);

    Mask bishop_pinners = bishop_atk & wait.bb[BISHOP];

    while (bishop_pinners) {
      Pos pinner = pop_pos(bishop_pinners);
      Mask pinner_mask = pos_mask(pinner);

      // Compute bishop ray as mask between pinner and king, including pinner

      Mask bishop_ray = between_mask(pinner, king_pos) | pinner_mask;

      // If only one piece in ray, it is pinned

      if (__builtin_popcountll(bishop_ray & act_occ) == 1) {
        pinned |= bishop_ray & act_occ;
        pin_rays[pinned_count++] = bishop_ray;
      }
    }

    Mask queen_atk = rook_atk | bishop_atk;

    Mask queen_pinners = queen_atk & wait.bb[QUEEN];

    while (queen_pinners) {
      Pos pinner = pop_pos(queen_pinners);
      Mask pinner_mask = pos_mask(pinner);

      // Compute queen ray as mask between pinner and king, including pinner

      Mask queen_ray = between_mask(pinner, king_pos) | pinner_mask;

      // If only one piece in ray, it is pinned

      if (__builtin_popcountll(queen_ray & act_occ) == 1) {
        pinned |= queen_ray & act_occ;
        pin_rays[pinned_count++] = queen_ray;
      }
    }

    return pinned_count;
  }

  u8 filter_pins(MoveList& pseudo, MoveList& moves, u8 pseudo_count, u8 count) {
    Mask pinned, pin_rays[8];
    u8 pinned_count = find_pins(pinned, pin_rays);

    // If no pinned pieces, copy all

    if (!pinned_count) {
      for (u8 i = 0; i < pseudo_count; i++) {
        moves[count++] = pseudo[i];
      }

      return count;
    }

    for (u8 i = 0; i < pseudo_count; i++) {
      Move move = pseudo[i];

      Pos from = move.from();

      Mask from_mask = pos_mask(from);

      // If position is not pinned, add move

      if (!(from_mask & pinned)) {
        moves[count++] = move;
        continue;
      }

      // Find ray of pin

      Mask pin_ray = 0ull;
      for (u8 j = 0; j < pinned_count; j++) {
        Mask _pin_ray = pin_rays[j];
        if (from_mask & _pin_ray) {
          pin_ray = _pin_ray;
          break;
        }
      }

      // If move is along pin ray, add move

      Pos to = move.to();

      Mask to_mask = pos_mask(to);

      if (pin_ray & to_mask) {
        moves[count++] = move;
      }
    }

    return count;
  }

  GenResult gen_legal(MoveList& moves) {
    u8 count = 0;


    Mask restrict;
    Check ct = find_checks(restrict);

    count = gen_king(moves, count, ct);

    switch (ct) {
      case NO_CHECK: {
        // Generate all moves, then filter out pins

        u8 pseudo_count = 0;
        MoveList pseudo;
        pseudo_count = gen_all(pseudo);
        count = filter_pins(pseudo, moves, pseudo_count, count);

        break;
      }
      case DIRECT_CHECK: {
        // Generate attacks capturing checker, then filter out pins

        u8 pseudo_count = 0;
        MoveList pseudo;
        pseudo_count = gen_atks(restrict, pseudo, 0);
        count = filter_pins(pseudo, moves, pseudo_count, count);

        break;
      }
      case SLIDE_CHECK: {
        // Generate moves capturing or blocking checker, then filter out pins

        u8 pseudo_count = 0;
        MoveList pseudo;
        pseudo_count = gen_blocks(restrict, pseudo);
        count = filter_pins(pseudo, moves, pseudo_count, count);

        break;
      }
      case DOUBLE_CHECK: {
        // Forced to move king

        break;
      }
    }

    return GenResult{count, ct};
  }
};



GenResult gen_legal(Game& game, MoveList& moves) {
  MoveGenerator mg(game, false);
  return mg.gen_legal(moves);
}

GenResult gen_non_quiet(Game& game, MoveList& moves) {
  MoveGenerator mg(game, true);
  return mg.gen_legal(moves);
}