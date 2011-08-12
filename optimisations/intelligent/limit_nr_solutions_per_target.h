#if !defined(OPTIMISATION_INTELLIGENT_LIMIT_NR_SOLUTIONS_PER_TARGET_POS_H)
#define OPTIMISATION_INTELLIGENT_LIMIT_NR_SOLUTIONS_PER_TARGET_POS_H

#include "pyslice.h"
#include "stipulation/help_play/play.h"

/* This module provides the slice types that limit the number of solutions per
 * target position if requested by the user
 */

/* Reset the maximum number of solutions per target position
 */
void reset_max_nr_solutions_per_target_position(void);

/* Reset the number of solutions per target position
 */
void reset_nr_solutions_per_target_position(void);

/* Reset status whether solving the current problem was affected because the limit
 * on the number of solutions per target position was reached.
 */
void reset_was_max_nr_solutions_per_target_position_reached(void);

/* Determine whether solving the current problem was affected because the limit
 * on the number of solutions per target position was reached.
 * @return true iff solving was affected
 */
boolean was_max_nr_solutions_per_target_position_reached(void);

/* Attempt to read the maximum number of solutions per target position
 * @param tok next input token
 * @return true iff the maximum number could be read from tok
 */
boolean read_max_nr_solutions_per_target_position(char const *tok);

/* Determine whether the maximum number of solutions per target position is
 * limited
 * @return true iff the maximum number is limited
 */
boolean is_max_nr_solutions_per_target_position_limited(void);

/* Allocate a STIntelligentSolutionsPerTargetPosCounter slice.
 * @return index of allocated slice
 */
slice_index alloc_intelligent_nr_solutions_per_target_position_counter_slice(void);

/* Determine whether a slice has just been solved with the move
 * by the non-starter
 * @param si slice identifier
 * @return whether there is a solution and (to some extent) why not
 */
has_solution_type
intelligent_nr_solutions_per_target_position_counter_has_solution(slice_index si);

/* Solve a slice
 * @param si slice index
 * @return whether there is a solution and (to some extent) why not
 */
has_solution_type
intelligent_nr_solutions_per_target_position_counter_solve(slice_index si);


/* Allocate a STIntelligentLimitNrSolutionsPerTargetPos slice.
 * @return index of allocated slice
 */
slice_index alloc_intelligent_limit_nr_solutions_per_target_position_slice(void);

/* Determine whether there is a solution in n half moves.
 * @param si slice index of slice being solved
 * @param n exact number of half moves until end state has to be reached
 * @return length of solution found, i.e.:
 *         n+4 the move leading to the current position has turned out
 *             to be illegal
 *         n+2 no solution found
 *         n   solution found
 */
stip_length_type
intelligent_limit_nr_solutions_per_target_position_can_help(slice_index si,
                                                            stip_length_type n);

/* Solve in a number of half-moves
 * @param si identifies slice
 * @param n exact number of half moves until end state has to be reached
 * @return length of solution found, i.e.:
 *         n+4 the move leading to the current position has turned out
 *             to be illegal
 *         n+2 no solution found
 *         n   solution found
 */
stip_length_type
intelligent_limit_nr_solutions_per_target_position_help(slice_index si,
                                                        stip_length_type n);

#endif