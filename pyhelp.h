#if !defined(PYHELP_H)
#define PYHELP_H

/* Interface for dynamically dispatching help play functions to slices
 * depending on the slice type
 */

#include "py.h"
#include "pystip.h"
#include "pytable.h"

/* Solve in a number of half-moves
 * @param si identifies slice
 * @param n number of half moves until end state has to be reached
 * @param side_at_move side at the move
 * @return true iff >=1 solution was found
 */
boolean help_solve_in_n(slice_index si, stip_length_type n, Side side_at_move);

/* Determine whether there is a solution in n half moves.
 * @param si slice index of slice being solved
 * @param n number of half moves until end state has to be reached
 * @param side_at_move side at move
 * @return true iff >= 1 solution has been found
 */
boolean help_has_solution_in_n(slice_index si,
                               stip_length_type n,
                               Side side_at_move);

/* Determine and write solution(s): add first moves to table (as
 * threats for the parent slice. First consult hash table.
 * @param continuations table where to add first moves
 * @param si slice index of slice being solved
 * @param n number of half moves until end state has to be reached
 * @param side_at_move side at move
 */
void help_solve_continuations_in_n(table continuations,
                                   slice_index si,
                                   stip_length_type n,
                                   Side side_at_move);

/* Determine the starter in a help stipulation in n half-moves
 * @param si identifies slice
 * @param n number of half-moves
 * @param return starting side
 */
Side help_starter_in_n(slice_index si, stip_length_type n);

/* Is there no chance left for the starting side at the move to win?
 * E.g. did the defender just capture that attacker's last potential
 * mating piece?
 * @param si slice index
 * @return true iff starter must resign
 */
boolean help_must_starter_resign(slice_index si);

/* Write a priori unsolvability (if any) of a slice (e.g. forced
 * reflex mates).
 * @param si slice index
 */
void help_write_unsolvability(slice_index si);

/* Determine whether a branch slice.has just been solved with the
 * just played move by the non-starter
 * @param si slice identifier
 * @return true iff the non-starting side has just solved
 */
boolean help_has_non_starter_solved(slice_index si);

/* Determine whether the starting side has made such a bad move that
 * it is clear without playing further that it is not going to win.
 * E.g. in s# or r#, has it taken the last potential mating piece of
 * the defender?
 * @param si slice identifier
 * @return true iff starter has lost
 */
boolean help_has_starter_apriori_lost(slice_index si);

/* Determine whether the attacker has won with his move just played
 * @param si slice identifier
 * @return true iff the starter has won
 */
boolean help_has_starter_won(slice_index si);

/* Determine whether the attacker has reached slice si's goal with his
 * move just played.
 * @param si slice identifier
 * @return true iff the starter reached the goal
 */
boolean help_has_starter_reached_goal(slice_index si);

/* Determine whether a side has reached the goal
 * @param just_moved side that has just moved
 * @param si slice index
 * @return true iff just_moved has reached the goal
 */
boolean help_is_goal_reached(Side just_moved, slice_index si);

#endif
