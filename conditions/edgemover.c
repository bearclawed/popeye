#include "conditions/edgemover.h"
#include "pydata.h"
#include "conditions/conditions.h"
#include "stipulation/pipe.h"
#include "stipulation/branch.h"
#include "solving/move_generator.h"
#include "solving/observation.h"
#include "debugging/trace.h"

#include <assert.h>

static boolean goes_to_the_edge(numecoup n)
{
  return !NoEdge(move_generation_stack[n].arrival);
}

/* Validate the geometry of observation according to Edgemover
 * @return true iff the observation is valid
 */
boolean edgemover_validate_observation_geometry(slice_index si)
{
  return (goes_to_the_edge(current_move[nbply]-1)
          && validate_observation_recursive(slices[si].next1));
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
stip_length_type edgemover_remove_illegal_moves_solve(slice_index si,
                                                      stip_length_type n)
{
  stip_length_type result;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u",n);
  TraceFunctionParamListEnd();

  move_generator_filter_moves(&goes_to_the_edge);

  result = solve(slices[si].next1,n);

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

static void insert_remover(slice_index si, stip_structure_traversal *st)
{
  boolean const (* const enabled)[nr_sides] = st->param;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  stip_traverse_structure_children(si,st);

  if ((*enabled)[slices[si].starter])
  {
    slice_index const prototype = alloc_pipe(STEdgeMoverRemoveIllegalMoves);
    branch_insert_slices_contextual(si,st->context,&prototype,1);
  }

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

/* Instrument the solvers with Patrol Chess
 * @param si identifies the root slice of the stipulation
 */
void stip_insert_edgemover(slice_index si)
{
  stip_structure_traversal st;
  boolean enabled[nr_sides] = { CondFlag[whiteedge], CondFlag[blackedge] };

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  TraceStipulation(si);

  stip_impose_starter(si,slices[si].starter);

  stip_structure_traversal_init(&st,&enabled);
  stip_structure_traversal_override_single(&st,
                                           STDoneGeneratingMoves,
                                           &insert_remover);
  stip_traverse_structure(si,&st);

  if (CondFlag[whiteedge])
    stip_instrument_observation_geometry_validation(si,
                                                    White,
                                                    STEdgeMoverRemoveIllegalMoves);
  if (CondFlag[blackedge])
    stip_instrument_observation_geometry_validation(si,
                                                    Black,
                                                    STEdgeMoverRemoveIllegalMoves);

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}
