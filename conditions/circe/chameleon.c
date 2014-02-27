#include "conditions/circe/chameleon.h"
#include "pieces/pieces.h"
#include "pieces/attributes/chameleon.h"
#include "pieces/walks/walks.h"
#include "stipulation/has_solution_type.h"
#include "stipulation/stipulation.h"
#include "conditions/circe/circe.h"
#include "debugging/trace.h"

#include "debugging/assert.h"

PieNam chameleon_circe_walk_sequence[PieceCount];

boolean chameleon_circe_is_squence_implicit;

/* Try to solve in n half-moves.
 * @param si slice index
 * @param n maximum number of half moves
 * @return length of solution found and written, i.e.:
 *            previous_move_is_illegal the move just played is illegal
 *            this_move_is_illegal     the move being played is illegal
 *            immobility_on_next_move  the moves just played led to an
 *                                     unintended immobility on the next move
 *            <=n+1 length of shortest solution found (n+1 only if in next
 *                                     branch)
 *            n+2 no solution found in this branch
 *            n+3 no solution found in next branch
 */
stip_length_type chameleon_circe_adapt_reborn_walk_solve(slice_index si,
                                                         stip_length_type n)
{
  stip_length_type result;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u",n);
  TraceFunctionParamListEnd();

  circe_rebirth_context_stack[circe_rebirth_context_stack_pointer].reborn_walk = chameleon_circe_walk_sequence[circe_rebirth_context_stack[circe_rebirth_context_stack_pointer].reborn_walk];
  circe_rebirth_context_stack[circe_rebirth_context_stack_pointer].relevant_walk = circe_rebirth_context_stack[circe_rebirth_context_stack_pointer].reborn_walk;

  result = solve(slices[si].next1,n);

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}
