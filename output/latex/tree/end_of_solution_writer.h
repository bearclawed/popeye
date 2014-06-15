#if !defined(OUTPUT_LATEX_END_OF_SOLUTION_WRITER_H)
#define OUTPUT_LATEX_END_OF_SOLUTION_WRITER_H

#include "solving/machinery/solve.h"

#include <stdio.h>

/* Allocate a STOutputLaTeXEndOfSolutionWriter slice.
 * @return index of allocated slice
 */
slice_index alloc_output_latex_tree_end_of_solution_writer_slice(FILE *file);

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
void output_latex_tree_end_of_solution_writer_solve(slice_index si);

#endif
