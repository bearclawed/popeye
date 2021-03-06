#include "conditions/einstein/en_passant.h"
#include "pieces/walks/pawns/en_passant.h"
#include "pieces/walks/classification.h"
#include "stipulation/pipe.h"
#include "solving/has_solution_type.h"
#include "stipulation/stipulation.h"
#include "stipulation/move.h"
#include "solving/move_effect_journal.h"
#include "solving/pipe.h"
#include "debugging/trace.h"
#include "pieces/pieces.h"

#include "debugging/assert.h"

/* Adjust en passant possibilities of the following move after a non-capturing
 * move
 * @param sq_multistep_departure departure square of pawn move
 */
static void adjust_ep_squares(square sq_multistep_departure)
{
  move_effect_journal_index_type const base = move_effect_journal_base[nbply];
  move_effect_journal_index_type const movement = base+move_effect_journal_index_offset_movement;
  piece_walk_type const pi_moving = move_effect_journal[movement].u.piece_movement.moving;

  switch (pi_moving)
  {
    case Pawn:
    case MarinePawn:
    {
      numvec const dir_forward = trait[nbply]==White ? dir_up : dir_down;
      square const sq_arrival = move_effect_journal[movement].u.piece_movement.to;
      if (sq_arrival-sq_multistep_departure==3*dir_forward)
      {
        move_effect_journal_do_remember_ep((sq_multistep_departure+sq_arrival+sq_arrival) / 3);
        move_effect_journal_do_remember_ep((sq_multistep_departure+sq_multistep_departure+sq_arrival) / 3);
      }
      break;
    }

    case ReversePawn:
    {
      numvec const dir_backward = trait[nbply]==White ? dir_down : dir_up;
      square const sq_arrival = move_effect_journal[movement].u.piece_movement.to;
      if (sq_arrival-sq_multistep_departure==3*dir_backward)
      {
        move_effect_journal_do_remember_ep((sq_multistep_departure+sq_arrival+sq_arrival) / 3);
        move_effect_journal_do_remember_ep((sq_multistep_departure+sq_multistep_departure+sq_arrival) / 3);
      }
      break;
    }

    default:
      break;
  }
}

/* Try to solve in solve_nr_remaining half-moves.
 * @param si slice index
 * @note assigns solve_result the length of solution found and written, i.e.:
 *            previous_move_is_illegal the move just played is illegal
 *            this_move_is_illegal     the move being played is illegal
 *            immobility_on_next_move  the moves just played led to an
 *                                     unintended immobility on the next move
 *            <=n+1 length of shortest solution found (n+1 only if in next
 *                                     branch)
 *            n+2 no solution found in this branch
 *            n+3 no solution found in next branch
 *            (with n denominating solve_nr_remaining)
 */
void einstein_en_passant_adjuster_solve(slice_index si)
{
  move_effect_journal_index_type const base = move_effect_journal_base[nbply];
  move_effect_journal_index_type const capture = base+move_effect_journal_index_offset_capture;
  move_effect_journal_index_type const movement = base+move_effect_journal_index_offset_movement;
  piece_walk_type const pi_moving = move_effect_journal[movement].u.piece_movement.moving;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  if (is_pawn(pi_moving)
      && move_effect_journal[capture].type==move_effect_no_piece_removal)
    adjust_ep_squares(move_effect_journal[movement].u.piece_movement.from);

  pipe_solve_delegate(si);

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

/* Instrument slices with promotee markers
 */
void solving_insert_einstein_en_passant_adjusters(slice_index si)
{
  TraceFunctionEntry(__func__);
  TraceFunctionParamListEnd();

  stip_instrument_moves(si,STEinsteinEnPassantAdjuster);

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}
