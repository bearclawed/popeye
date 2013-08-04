#include "optimisations/detect_retraction.h"
#include "pydata.h"
#include "pyproc.h"
#include "solving/move_generator.h"
#include "solving/move_effect_journal.h"
#include "stipulation/stipulation.h"
#include "stipulation/pipe.h"
#include "stipulation/has_solution_type.h"
#include "stipulation/proxy.h"
#include "stipulation/branch.h"
#include "debugging/trace.h"

#include <assert.h>

typedef struct
{
    boolean does_retract_refute;
    boolean can_priorise;
} instrumentation_state;

static void optimise_move(slice_index si, stip_structure_traversal *st)
{
  instrumentation_state const * const state = st->param;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  switch (st->context)
  {
    case stip_traversal_context_attack:
    case stip_traversal_context_defense:
      if (state->does_retract_refute)
      {
        slice_index const prototype = alloc_pipe(STDetectMoveRetracted);
        branch_insert_slices_contextual(si,st->context,&prototype,1);
      }
      break;

    case stip_traversal_context_help:
      /* don't suppress longer versions of short solutions */
      break;

    default:
      break;
  }

  stip_traverse_structure_children(si,st);

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

static void enter_branch(slice_index si, stip_structure_traversal *st)
{
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  if (slices[si].u.branch.min_length>slack_length+1)
    /* repeating the position may be sound tactics in branches with a
     * minimum length */
    stip_traverse_structure_children(si,st);
  else
  {
    instrumentation_state * const state = st->param;
    boolean const save_exact = state->does_retract_refute;
    boolean const save_can_priorise = state->can_priorise;

    state->does_retract_refute = true;
    state->can_priorise = true;
    stip_traverse_structure_children(si,st);
    state->can_priorise = save_can_priorise;
    state->does_retract_refute = save_exact;
  }

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

static void remember_not_to_priorise(slice_index si,
                                     stip_structure_traversal *st)
{
  instrumentation_state * const state = st->param;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  stip_traverse_structure_children(si,st);
  state->can_priorise = false;

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

static void priorise(slice_index si, stip_structure_traversal *st)
{
  instrumentation_state const * const state = st->param;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  if (st->activity==stip_traversal_activity_testing)
    switch (st->context)
    {
      case stip_traversal_context_attack:
      case stip_traversal_context_defense:
        if (state->can_priorise)
        {
          slice_index const prototype = alloc_pipe(STRetractionPrioriser);
          branch_insert_slices_contextual(si,st->context,&prototype,1);
        }
        break;

      default:
        break;
    }

  stip_traverse_structure_children(si,st);

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

/* Optimise move generation by inserting orthodox mating move generators
 * @param si identifies the root slice of the stipulation
 */
void solving_optimise_by_detecting_retracted_moves(slice_index si)
{
  stip_structure_traversal st;
  instrumentation_state state = { false, false };

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  TraceStipulation(si);

  stip_structure_traversal_init(&st,&state);
  stip_structure_traversal_override_single(&st,STAttackAdapter,&enter_branch);
  stip_structure_traversal_override_single(&st,STDefenseAdapter,&enter_branch);
  stip_structure_traversal_override_single(&st,STGeneratingMoves,&priorise);
  stip_structure_traversal_override_single(&st,STOrthodoxMatingMoveGenerator,&remember_not_to_priorise);
  stip_structure_traversal_override_single(&st,STKillerMoveFinalDefenseMove,&remember_not_to_priorise);
  stip_structure_traversal_override_single(&st,STMove,&optimise_move);
  stip_traverse_structure(si,&st);

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

static boolean is_retractable_effect(move_effect_type type)
{
  return type==move_effect_imitator_movement || type==move_effect_king_square_movement;
}

static boolean retracting_opponent_move(void)
{
  boolean result = false;
  ply const parent = parent_ply[nbply];

  TraceFunctionEntry(__func__);
  TraceFunctionParamListEnd();

  if (trait[nbply]!=trait[parent]
      && move_effect_journal_index_offset_capture==0)
  {
    move_effect_journal_index_type const parent_base = move_effect_journal_top[parent-1];
    move_effect_journal_index_type const parent_top = move_effect_journal_top[parent];
    move_effect_journal_index_type const base = move_effect_journal_top[nbply-1];
    move_effect_journal_index_type const top = move_effect_journal_top[nbply];
    if (parent_top-parent_base==top-base
        && move_effect_journal[parent_base].type==move_effect_no_piece_removal
        && move_effect_journal[base].type==move_effect_no_piece_removal
        && move_effect_journal[parent_base+1].type==move_effect_piece_movement
        && move_effect_journal[base+1].type==move_effect_piece_movement
        && move_effect_journal[parent_base+1].u.piece_movement.from==move_effect_journal[base+1].u.piece_movement.to
        && move_effect_journal[parent_base+1].u.piece_movement.to==move_effect_journal[base+1].u.piece_movement.from
        && move_effect_journal[parent_base+1].u.piece_movement.moving==move_effect_journal[base+1].u.piece_movement.moving
        && move_effect_journal[parent_base+1].u.piece_movement.movingspec==move_effect_journal[base+1].u.piece_movement.movingspec)
    {
      move_effect_journal_index_type curr;

      result = true;

      for (curr = base+move_effect_journal_index_offset_other_effects; curr<top; ++curr)
        if (!is_retractable_effect(move_effect_journal[curr].type))
        {
          result = false;
          break;
        }
    }
  }

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

/* Try to solve in n half-moves.
 * @param si slice index
 * @param n maximum number of half moves
 * @return length of solution found and written, i.e.:
 *            previous_move_is_illegal the move just played (or being played)
 *                                     is illegal
 *            immobility_on_next_move  the moves just played led to an
 *                                     unintended immobility on the next move
 *            <=n+1 length of shortest solution found (n+1 only if in next
 *                                     branch)
 *            n+2 no solution found in this branch
 *            n+3 no solution found in next branch
 */
stip_length_type detect_move_retracted_solve(slice_index si, stip_length_type n)
{
  stip_length_type result;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u",n);
  TraceFunctionParamListEnd();

  if (retracting_opponent_move())
    result = n+2;
  else
    result = solve(slices[si].next1,n);

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

static boolean is_previous_move_retractable(void)
{
  boolean result = false;
  ply const parent = parent_ply[nbply];

  move_effect_journal_index_type const parent_base = move_effect_journal_top[parent-1];
  move_effect_journal_index_type const parent_top = move_effect_journal_top[parent];
  if (move_effect_journal[parent_base].type==move_effect_no_piece_removal
      && move_effect_journal[parent_base+1].type==move_effect_piece_movement)
  {
    move_effect_journal_index_type curr;

    result = true;

    for (curr = parent_base+move_effect_journal_index_offset_other_effects; curr<parent_top; ++curr)
      if (!is_retractable_effect(move_effect_journal[curr].type))
      {
        result = false;
        break;
      }
  }

  return result;
}

/* Try to solve in n half-moves.
 * @param si slice index
 * @param n maximum number of half moves
 * @return length of solution found and written, i.e.:
 *            previous_move_is_illegal the move just played (or being played)
 *                                     is illegal
 *            immobility_on_next_move  the moves just played led to an
 *                                     unintended immobility on the next move
 *            <=n+1 length of shortest solution found (n+1 only if in next
 *                                     branch)
 *            n+2 no solution found in this branch
 *            n+3 no solution found in next branch
 */
stip_length_type priorise_retraction_solve(slice_index si, stip_length_type n)
{
  stip_length_type result;
  ply const parent = parent_ply[nbply];
  square const sq_arrival = move_generation_stack[current_move[parent]-1].arrival;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u",n);
  TraceFunctionParamListEnd();

  if (trait[nbply]!=trait[parent]
      && move_effect_journal_index_offset_capture==0
      && is_previous_move_retractable())
  {
    square const sq_departure = move_generation_stack[current_move[parent]-1].departure;
    numecoup i;
    for (i = current_move[nbply]-1; i>current_move[nbply-1]-1; --i)
      if (move_generation_stack[i].departure==sq_arrival
          && move_generation_stack[i].arrival==sq_departure)
      {
        move_generator_priorise(i);
        break;
      }
  }

  result = solve(slices[si].next1,n);

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}