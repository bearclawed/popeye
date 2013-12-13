#include "conditions/anticirce/clone.h"
#include "conditions/circe/circe.h"
#include "solving/move_effect_journal.h"
#include "debugging/trace.h"

#include <assert.h>

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
stip_length_type anti_clone_circe_determine_reborn_piece_solve(slice_index si,
                                                        	      stip_length_type n)
{
  stip_length_type result;
  move_effect_journal_index_type const base = move_effect_journal_base[nbply];
  move_effect_journal_index_type const capture = base+move_effect_journal_index_offset_capture;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u",n);
  TraceFunctionParamListEnd();

  if (!TSTFLAG(move_effect_journal[capture].u.piece_removal.removedspec,Royal))
    circe_rebirth_context_stack[circe_rebirth_context_stack_pointer].reborn_walk = move_effect_journal[capture].u.piece_removal.removed;

  circe_rebirth_context_stack[circe_rebirth_context_stack_pointer].relevant_walk = circe_rebirth_context_stack[circe_rebirth_context_stack_pointer].reborn_walk;

  result = solve(slices[si].next1,n);

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}