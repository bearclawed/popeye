#if !defined(PIECES_ATTRIBUTES_MAGIC_H)
#define PIECES_ATTRIBUTES_MAGIC_H

/* This module implements magic pieces */

#include "pieces/pieces.h"
#include "solving/solve.h"

/* Can a specific type of (fairy) piece be magic?
 * @param p type of piece
 * @return true iff pieces of type p can be magic
 */
boolean magic_is_piece_supported(PieNam p);

boolean magic_enforce_observer(slice_index si);

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
stip_length_type magic_views_initialiser_solve(slice_index si,
                                                stip_length_type n);

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
stip_length_type magic_pieces_recolorer_solve(slice_index si,
                                               stip_length_type n);

/* Instrument a stipulation
 * @param si identifies root slice of stipulation
 */
void stip_insert_magic_pieces_recolorers(slice_index si);

#endif
