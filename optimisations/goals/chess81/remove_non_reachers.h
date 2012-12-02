#if !defined(OPTIMISATION_GOALS_CHESS81_REMOVE_NON_REACHERS_H)
#define OPTIMISATION_GOALS_CHESS81_REMOVE_NON_REACHERS_H

#include "solving/solve.h"

/* This module provides slice type STChess81RemoveNonReachers.
 * Slices of this type optimise by removing moves that are not moves reaching
 * that goal.
 */

/* Allocate a STChess81RemoveNonReachers slice.
 * @return index of allocated slice
 */
slice_index alloc_chess81_remove_non_reachers_slice(void);

/* Try to solve in n half-moves.
 * @param si slice index
 * @param n maximum number of half moves
 * @return length of solution found and written, i.e.:
 *            slack_length-2 the move just played or being played is illegal
 *            <=n length of shortest solution found
 *            n+2 no solution found
 */
stip_length_type chess81_remove_non_reachers_solve(slice_index si,
                                                   stip_length_type n);

#endif
