#include "conditions/marscirce/anti.h"
#include "pydata.h"
#include "stipulation/stipulation.h"
#include "stipulation/pipe.h"
#include "stipulation/stipulation.h"
#include "stipulation/has_solution_type.h"
#include "stipulation/move.h"
#include "solving/en_passant.h"
#include "solving/move_effect_journal.h"
#include "conditions/marscirce/marscirce.h"
#include "debugging/trace.h"

#include <assert.h>
#include <stdlib.h>

/* Generate moves for a piece with a specific walk from a specific departure
 * square.
 * @param side side for which to generate moves for
 * @param p indicates the walk according to which to generate moves
 * @param sq_departure departure square of moves to be generated
 * @note the piece on the departure square need not necessarily have walk p
 */
void antimars_generate_moves(Side side, piece p, square sq_departure)
{
  TraceFunctionEntry(__func__);
  TraceEnumerator(Side,side,"");
  TracePiece(p);
  TraceSquare(sq_departure);
  TraceFunctionParamListEnd();

  {
    square const sq_rebirth = (*marsrenai)(abs(p),
                                           spec[sq_departure],
                                           sq_departure,initsquare,initsquare,
                                           advers(side));

    if (sq_rebirth==sq_departure)
      gen_piece_aux(side,sq_departure,abs(p));
    else
    {
      marscirce_generate_captures(side,p,sq_departure,sq_departure);

      if (is_square_empty(sq_rebirth))
      {
        occupy_square(sq_rebirth,get_walk_of_piece_on_square(sq_departure),spec[sq_departure]);
        empty_square(sq_departure);

        marscirce_generate_non_captures(side,p,sq_rebirth,sq_departure);

        occupy_square(sq_departure,get_walk_of_piece_on_square(sq_rebirth),spec[sq_rebirth]);
        empty_square(sq_rebirth);
      }
    }
  }

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

static square adjust(void)
{
  square result = initsquare;
  move_effect_journal_index_type const base = move_effect_journal_top[nbply-1];
  move_effect_journal_index_type const capture = base+move_effect_journal_index_offset_capture;
  move_effect_journal_index_type const movement = base+move_effect_journal_index_offset_movement;
  square const sq_arrival = move_effect_journal[movement].u.piece_movement.to;
  PieNam const pi_arriving = get_walk_of_piece_on_square(sq_arrival);

  if (is_pawn(pi_arriving)
      && move_effect_journal[capture].type==move_effect_no_piece_removal)
  {
    square const sq_departure = move_effect_journal[movement].u.piece_movement.from;
    square const sq_rebirth = (*marsrenai)(pi_arriving,
                                           spec[sq_arrival],
                                           sq_departure,
                                           initsquare,
                                           initsquare,
                                           advers(trait[nbply]));
    result = en_passant_find_potential(sq_rebirth);

    /* In positions like pieces black pe6 white pd5, we can't tell whether
     * the black pawn's move to e5 is a double step.
     * As the ep capturer normally has the burden of proof, let's disallow
     * ep capture in that case for now.
     */
    if (result==sq_departure)
      result = initsquare;
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
stip_length_type antimars_en_passant_adjuster_solve(slice_index si,
                                                     stip_length_type n)
{
  stip_length_type result;
  square multipstep_over;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u",n);
  TraceFunctionParamListEnd();

  multipstep_over = adjust();
  if (multipstep_over==initsquare)
    result = solve(slices[si].next1,n);
  else
  {
    en_passant_remember_multistep_over(0,multipstep_over);
    result = solve(slices[si].next1,n);
    en_passant_forget_multistep();
  }

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

/* Instrument slices with Anti-Mars-Circe
 */
void stip_insert_antimars_en_passant_adjusters(slice_index si)
{
  TraceFunctionEntry(__func__);
  TraceFunctionParamListEnd();

  stip_instrument_moves(si,STAntiMarsCirceEnPassantAdjuster);

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}
