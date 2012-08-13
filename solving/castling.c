#include "solving/castling.h"
#include "pydata.h"
#include "pyproc.h"
#include "stipulation/pipe.h"
#include "stipulation/proxy.h"
#include "stipulation/pipe.h"
#include "stipulation/fork.h"
#include "stipulation/branch.h"
#include "stipulation/battle_play/branch.h"
#include "stipulation/help_play/branch.h"
#include "stipulation/stipulation.h"
#include "stipulation/move_player.h"
#include "debugging/trace.h"

#include <assert.h>
#include <stdlib.h>

castling_flag_type castling_flag[maxply+2];

boolean castling_supported;

castling_flag_type castling_mutual_exclusive[nr_sides][2];

/* Restore the castling rights of a piece
 * @param sq_arrival position of piece for which to restore castling rights
 */
void restore_castling_rights(square sq_arrival)
{
  if (castling_supported)
  {
    piece const pi_arrived = e[sq_arrival];
    Flags const spec_arrived = spec[sq_arrival];

    if (abs(pi_arrived) == Rook)
    {
      if (TSTFLAG(spec_arrived, White)) {
        if (sq_arrival == square_h1)
          /* white rook reborn on h1 */
          SETCASTLINGFLAGMASK(nbply,White,rh_cancastle);
        else if (sq_arrival == square_a1)
          /* white rook reborn on a1 */
          SETCASTLINGFLAGMASK(nbply,White,ra_cancastle);
      }
      if (TSTFLAG(spec_arrived, Black)) {
        if (sq_arrival == square_h8)
          /* black rook reborn on h8 */
          SETCASTLINGFLAGMASK(nbply,Black,rh_cancastle);
        else if (sq_arrival == square_a8)
          /* black rook reborn on a8 */
          SETCASTLINGFLAGMASK(nbply,Black,ra_cancastle);
      }
    }

    else if (abs(pi_arrived) == King) {
      if (TSTFLAG(spec_arrived, White)
          && sq_arrival == square_e1
          && (!CondFlag[dynasty] || nbpiece[roib]==1))
        /* white king reborn on e1 */
        SETCASTLINGFLAGMASK(nbply,White,k_cancastle);
      else if (TSTFLAG(spec_arrived, Black)
               && sq_arrival == square_e8
               && (!CondFlag[dynasty] || nbpiece[roin]==1))
        /* black king reborn on e8 */
        SETCASTLINGFLAGMASK(nbply,Black,k_cancastle);
    }
  }
}

static square square_departure;
static square square_arrival;

/* Allocate a STCastlingIntermediateMoveGenerator slice.
 * @return index of allocated slice
 */
slice_index alloc_castling_intermediate_move_generator_slice(void)
{
  slice_index result;

  TraceFunctionEntry(__func__);
  TraceFunctionParamListEnd();

  result = alloc_pipe(STCastlingIntermediateMoveGenerator);

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

/* Initialise the next1 move generation
 * @param sq_departure departure square of move to be generated
 * @param sq_arrival arrival square of move to be generated
 */
void castling_intermediate_move_generator_init_next(square sq_departure,
                                                    square sq_arrival)
{
  TraceFunctionEntry(__func__);
  TraceSquare(sq_departure);
  TraceSquare(sq_arrival);
  TraceFunctionParamListEnd();

  /* avoid concurrent generations */
  assert(square_departure==initsquare);

  square_departure = sq_departure;
  square_arrival = sq_arrival;

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

/* Try to solve in n half-moves after a defense.
 * @param si slice index
 * @param n maximum number of half moves until end state has to be reached
 * @return length of solution found and written, i.e.:
 *            slack_length-2 defense has turned out to be illegal
 *            <=n length of shortest solution found
 *            n+2 no solution found
 */
stip_length_type castling_intermediate_move_generator_attack(slice_index si,
                                                             stip_length_type n)
{
  stip_length_type result;
  slice_index const next = slices[si].next1;
  numecoup const save_repere = current_move[parent_ply[nbply]];

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u",n);
  TraceFunctionParamListEnd();

  /* We work within a ply for which moves are being generated right now.
   * That's why we don't do nextply()/finply() - we just trick our successor
   * slices into believing that this intermediate move is the only one in the
   * ply.
   */
  current_move[parent_ply[nbply]] = current_move[nbply];
  empile(square_departure,square_arrival,square_arrival);
  result = attack(next,n);
  current_move[parent_ply[nbply]] = save_repere;

  /* clean up after ourselves */
  square_departure = initsquare;

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

static void castle(square sq_departure, square sq_arrival,
                   square sq_partner_departure, square sq_partner_arrival)
{
  TraceFunctionEntry(__func__);
  TraceSquare(sq_departure);
  TraceSquare(sq_arrival);
  TraceSquare(sq_partner_departure);
  TraceSquare(sq_partner_arrival);
  TraceFunctionParamListEnd();

  jouespec[nbply] = spec[sq_departure];
  jouearr[nbply] = e[sq_departure];

  assert(sq_arrival!=nullsquare);

  pjoue[nbply] = e[sq_departure];

  e[sq_partner_arrival] = e[sq_partner_departure];
  spec[sq_partner_arrival] = spec[sq_partner_departure];

  e[sq_partner_departure] = vide;
  CLEARFL(spec[sq_partner_departure]);

  e[sq_departure] = vide;
  spec[sq_departure]= 0;

  e[sq_arrival] = jouearr[nbply];
  spec[sq_arrival] = jouespec[nbply];

  pprise[nbply] = vide;
  pprispec[nbply] = 0;

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

static void uncastle(square sq_departure, square sq_arrival,
                     square sq_partner_departure, square sq_partner_arrival)
{
  TraceFunctionEntry(__func__);
  TraceSquare(sq_departure);
  TraceSquare(sq_arrival);
  TraceSquare(sq_partner_departure);
  TraceSquare(sq_partner_arrival);
  TraceFunctionParamListEnd();

  e[sq_partner_departure] = e[sq_partner_arrival];
  spec[sq_partner_departure] = spec[sq_partner_arrival];

  e[sq_partner_arrival] = vide;
  CLEARFL(spec[sq_partner_arrival]);

  e[sq_arrival] = vide;
  spec[sq_arrival] = 0;

  e[sq_departure] = pjoue[nbply];
  spec[sq_departure] = jouespec[nbply];

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

/* Try to solve in n half-moves after a defense.
 * @param si slice index
 * @param n maximum number of half moves until goal
 * @return length of solution found and written, i.e.:
 *            slack_length-2 defense has turned out to be illegal
 *            <=n length of shortest solution found
 *            n+2 no solution found
 */
stip_length_type castling_player_attack(slice_index si, stip_length_type n)
{
  stip_length_type result;
  numecoup const coup_id = current_move[nbply];
  move_generation_elmt const * const move_gen_top = move_generation_stack+coup_id;
  square const sq_departure = move_gen_top->departure;
  square const sq_arrival = move_gen_top->arrival;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u",n);
  TraceFunctionParamListEnd();

  switch (move_gen_top->capture)
  {
    case kingside_castling:
    {
      square const sq_partner_departure = sq_departure+3*dir_right;
      square const sq_partner_arrival = sq_departure+dir_right;

      castle(sq_departure,sq_arrival,sq_partner_departure,sq_partner_arrival);
      result = attack(slices[si].next2,n);
      uncastle(sq_departure,sq_arrival,sq_partner_departure,sq_partner_arrival);

      break;
    }

    case queenside_castling:
    {
      square const sq_partner_departure = sq_departure+4*dir_left;
      square const sq_partner_arrival = sq_departure+dir_left;

      castle(sq_departure,sq_arrival,sq_partner_departure,sq_partner_arrival);
      result = attack(slices[si].next2,n);
      uncastle(sq_departure,sq_arrival,sq_partner_departure,sq_partner_arrival);

      break;
    }

    default:
      result = attack(slices[si].next1,n);
      break;
  }

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

/* Try to defend after an attacking move
 * When invoked with some n, the function assumes that the key doesn't
 * solve in less than n half moves.
 * @param si slice index
 * @param n maximum number of half moves until end state has to be reached
 * @return <slack_length - no legal defense found
 *         <=n solved  - <=acceptable number of refutations found
 *                       return value is maximum number of moves
 *                       (incl. defense) needed
 *         n+2 refuted - >acceptable number of refutations found
 */
stip_length_type castling_player_defend(slice_index si, stip_length_type n)
{
  stip_length_type result;
  numecoup const coup_id = current_move[nbply];
  move_generation_elmt const * const move_gen_top = move_generation_stack+coup_id;
  square const sq_departure = move_gen_top->departure;
  square const sq_arrival = move_gen_top->arrival;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u",n);
  TraceFunctionParamListEnd();

  switch (move_gen_top->capture)
  {
    case kingside_castling:
    {
      square const sq_partner_departure = sq_departure+3*dir_right;
      square const sq_partner_arrival = sq_departure+dir_right;

      castle(sq_departure,sq_arrival,sq_partner_departure,sq_partner_arrival);
      result = defend(slices[si].next2,n);
      uncastle(sq_departure,sq_arrival,sq_partner_departure,sq_partner_arrival);

      break;
    }

    case queenside_castling:
    {
      square const sq_partner_departure = sq_departure+4*dir_left;
      square const sq_partner_arrival = sq_departure+dir_left;

      castle(sq_departure,sq_arrival,sq_partner_departure,sq_partner_arrival);
      result = defend(slices[si].next2,n);
      uncastle(sq_departure,sq_arrival,sq_partner_departure,sq_partner_arrival);

      break;
    }

    default:
      result = defend(slices[si].next1,n);
      break;
  }

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

/* Remove the castling rights according to the current move
 */
static void remove_castling_rights(Side trait_ply)
{
  if (castling_supported)
  {
    square const sq_departure = move_generation_stack[current_move[nbply]].departure;
    square const sq_capture = move_generation_stack[current_move[nbply]].capture;
    square const sq_arrival = move_generation_stack[current_move[nbply]].arrival;

    if (sq_capture==kingside_castling)
      CLRCASTLINGFLAGMASK(nbply,trait_ply,k_castling);
    if (sq_capture==queenside_castling)
      CLRCASTLINGFLAGMASK(nbply,trait_ply,q_castling);

    /* pieces vacating a1, h1, a8, h8 */
    if (sq_departure == square_h1)
      CLRCASTLINGFLAGMASK(nbply,White,rh_cancastle);
    else if (sq_departure == square_a1)
      CLRCASTLINGFLAGMASK(nbply,White,ra_cancastle);
    else if (sq_departure == square_h8)
      CLRCASTLINGFLAGMASK(nbply,Black,rh_cancastle);
    else if (sq_departure == square_a8)
      CLRCASTLINGFLAGMASK(nbply,Black,ra_cancastle);

    if (sq_departure==prev_king_square[White][nbply])
      CLRCASTLINGFLAGMASK(nbply,White,k_cancastle);
    if (sq_departure==prev_king_square[Black][nbply])
      CLRCASTLINGFLAGMASK(nbply,Black,k_cancastle);

    /* pieces arriving at a1, h1, a8, h8 and possibly capturing a rook */
    if (sq_arrival == square_h1)
      CLRCASTLINGFLAGMASK(nbply,White,rh_cancastle);
    else if (sq_arrival == square_a1)
      CLRCASTLINGFLAGMASK(nbply,White,ra_cancastle);
    else if (sq_arrival == square_h8)
      CLRCASTLINGFLAGMASK(nbply,Black,rh_cancastle);
    else if (sq_arrival == square_a8)
      CLRCASTLINGFLAGMASK(nbply,Black,ra_cancastle);
  }
}

/* Try to solve in n half-moves after a defense.
 * @param si slice index
 * @param n maximum number of half moves until end state has to be reached
 * @return length of solution found and written, i.e.:
 *            slack_length-2 defense has turned out to be illegal
 *            <=n length of shortest solution found
 *            n+2 no solution found
 */
stip_length_type castling_rights_remover_attack(slice_index si,
                                                stip_length_type n)
{
  stip_length_type result;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u",n);
  TraceFunctionParamListEnd();

  remove_castling_rights(slices[si].starter);
  result = attack(slices[si].next1,n);
  castling_flag[nbply] = castling_flag[parent_ply[nbply]];

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

/* Try to solve in n half-moves after a defense.
 * @param si slice index
 * @param n maximum number of half moves until end state has to be reached
 * @return length of solution found and written, i.e.:
 *            slack_length-2 defense has turned out to be illegal
 *            <=n length of shortest solution found
 *            n+2 no solution found
 */
stip_length_type castling_rights_remover_defend(slice_index si,
                                                stip_length_type n)
{
  stip_length_type result;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u",n);
  TraceFunctionParamListEnd();

  remove_castling_rights(slices[si].starter);
  result = defend(slices[si].next1,n);
  castling_flag[nbply] = castling_flag[parent_ply[nbply]];

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

typedef struct
{
    slice_type const type;
    slice_index landing;
} alternative_move_player_installation_state_type;

static void insert_handler(slice_index si, stip_structure_traversal *st)
{
  alternative_move_player_installation_state_type const * const state = st->param;
  slice_index const proxy = alloc_proxy_slice();
  slice_index const prototype = alloc_fork_slice(state->type,proxy);

  assert(state->landing!=no_slice);
  link_to_branch(proxy,state->landing);

  switch (st->context)
  {
    case stip_traversal_context_attack:
      attack_branch_insert_slices(si,&prototype,1);
      break;

    case stip_traversal_context_defense:
      defense_branch_insert_slices(si,&prototype,1);
      break;

    case stip_traversal_context_help:
      help_branch_insert_slices(si,&prototype,1);
      break;

    default:
      assert(0);
      break;
  }
}

static void insert_landing(slice_index si, stip_structure_traversal *st)
{
  slice_index const prototype = alloc_pipe(STLandingAfterMovingPieceMovement);

  switch (st->context)
  {
    case stip_traversal_context_attack:
      attack_branch_insert_slices(si,&prototype,1);
      break;

    case stip_traversal_context_defense:
      defense_branch_insert_slices(si,&prototype,1);
      break;

    case stip_traversal_context_help:
      help_branch_insert_slices(si,&prototype,1);
      break;

    default:
      assert(0);
      break;
  }
}

static void instrument_move(slice_index si, stip_structure_traversal *st)
{
  alternative_move_player_installation_state_type * const state = st->param;
  slice_index const save_landing = state->landing;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  state->landing = no_slice;
  insert_landing(si,st);

  stip_traverse_structure_children_pipe(si,st);

  insert_handler(si,st);
  state->landing = save_landing;

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

static void remember_landing(slice_index si, stip_structure_traversal *st)
{
  alternative_move_player_installation_state_type * const state = st->param;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  assert(state->landing==no_slice);
  stip_traverse_structure_children_pipe(si,st);
  assert(state->landing==no_slice);
  state->landing = si;

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

/* Instrument a stipulation
 * @param si identifies root slice of stipulation
 */
void insert_alternative_move_players(slice_index si, slice_type type)
{
  stip_structure_traversal st;
  alternative_move_player_installation_state_type state = { type, no_slice };

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  stip_structure_traversal_init(&st,&state);
  stip_structure_traversal_override_single(&st,
                                           STMove,
                                           &instrument_move);
  stip_structure_traversal_override_single(&st,
                                           STReplayingMoves,
                                           &instrument_move);
  stip_structure_traversal_override_single(&st,
                                           STLandingAfterMovingPieceMovement,
                                           &remember_landing);
  stip_traverse_structure(si,&st);

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

/* Instrument slices with move tracers
 */
void stip_insert_castling(slice_index si)
{
  TraceFunctionEntry(__func__);
  TraceFunctionParamListEnd();

  insert_alternative_move_players(si,STCastlingPlayer);
  stip_instrument_moves_no_replay(si,STCastlingRightsRemover);

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

static void adjust_mutual_castling_rights(Side trait_ply)
{
  switch (move_generation_stack[current_move[nbply]].capture)
  {
    case kingside_castling:
      CLRCASTLINGFLAGMASK(nbply,advers(trait_ply),
                          castling_mutual_exclusive[trait_ply][kingside_castling-min_castling]);
      break;

    case queenside_castling:
      CLRCASTLINGFLAGMASK(nbply,advers(trait_ply),
                          castling_mutual_exclusive[trait_ply][queenside_castling-min_castling]);
      break;
  }
}

/* Try to solve in n half-moves after a defense.
 * @param si slice index
 * @param n maximum number of half moves until goal
 * @return length of solution found and written, i.e.:
 *            slack_length-2 defense has turned out to be illegal
 *            <=n length of shortest solution found
 *            n+2 no solution found
 */
stip_length_type mutual_castling_rights_adjuster_attack(slice_index si,
                                                        stip_length_type n)
{
  stip_length_type result;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u",n);
  TraceFunctionParamListEnd();

  adjust_mutual_castling_rights(slices[si].starter);
  result = attack(slices[si].next1,n);

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

/* Try to defend after an attacking move
 * When invoked with some n, the function assumes that the key doesn't
 * solve in less than n half moves.
 * @param si slice index
 * @param n maximum number of half moves until end state has to be reached
 * @return <slack_length - no legal defense found
 *         <=n solved  - <=acceptable number of refutations found
 *                       return value is maximum number of moves
 *                       (incl. defense) needed
 *         n+2 refuted - >acceptable number of refutations found
 */
stip_length_type mutual_castling_rights_adjuster_defend(slice_index si,
                                                        stip_length_type n)
{
  stip_length_type result;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u",n);
  TraceFunctionParamListEnd();

  adjust_mutual_castling_rights(slices[si].starter);
  result = defend(slices[si].next1,n);

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

/* Instrument slices with move tracers
 */
void stip_insert_mutual_castling_rights_adjusters(slice_index si)
{
  TraceFunctionEntry(__func__);
  TraceFunctionParamListEnd();

  stip_instrument_moves(si,STMutualCastlingRightsAdjuster);

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}