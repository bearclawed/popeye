#if !defined(PYDIRECT_H)
#define PYDIRECT_H

/* Interface for dynamically dispatching direct play functions to slices
 * depending on the slice type
 */

#include "py.h"
#include "pyslice.h"
#include "pytable.h"

#include <limits.h>

#define ENUMERATION_TYPENAME attack_result_type
#define ENUMERATORS \
  ENUMERATOR(attack_has_reached_deadend),       \
    ENUMERATOR(attack_has_full_length_play),    \
    ENUMERATOR(attack_has_solved_next_branch)

#define ENUMERATION_DECLARE

#include "pyenum.h"

/* Try to defend after an attempted key move at root level
 * @param table table where to add refutations
 * @param si slice index
 * @return success of key move
 */
attack_result_type direct_defender_root_defend(table refutations,
                                               slice_index si);

/* Determine whether the defense just played defends against the threats.
 * @param threats table containing the threats
 * @param len_threat length of threat(s) in table threats
 * @param si slice index
 * @param n maximum number of half moves until goal
 * @param curr_max_nr_nontrivial remaining maximum number of
 *                               allowed non-trivial variations
 * @return true iff the defense defends against at least one of the
 *         threats
 */
boolean direct_are_threats_refuted_in_n(table threats,
                                        stip_length_type len_threat,
                                        slice_index si,
                                        stip_length_type n,
                                        unsigned int curr_max_nr_nontrivial);

/* Determine whether there is a solution in n half moves.
 * @param si slice index of slice being solved
 * @param n maximum number of half moves until end state has to be reached
 * @param curr_max_nr_nontrivial remaining maximum number of
 *                               allowed non-trivial variations
 * @return whether there is a solution and (to some extent) why not
 */
has_solution_type direct_has_solution_in_n(slice_index si,
                                           stip_length_type n,
                                           unsigned int curr_max_nr_nontrivial);

/* Determine and write solution(s): add first moves to table (as
 * threats for the parent slice. First consult hash table.
 * @param continuations table where to add first moves
 * @param si slice index of slice being solved
 * @param n maximum number of half moves until end state has to be reached
 */
void direct_solve_continuations_in_n(table continuations,
                                     slice_index si,
                                     stip_length_type n);

/* Determine and write the threats after the move that has just been
 * played.
 * @param threats table where to add threats
 * @param si slice index
 * @param n maximum number of half moves until goal
 * @return length of threats
 *         (n-slack_length_direct)%2 if the attacker has something
 *           stronger than threats (i.e. has delivered check)
 *         n+2 if there is no threat
 */
stip_length_type direct_solve_threats_in_n(table threats,
                                           slice_index si,
                                           stip_length_type n);

/* Try to defend after an attempted key move at non-root level
 * @param si slice index
 * @param n maximum number of half moves until end state has to be reached
 * @param curr_max_nr_nontrivial remaining maximum number of
 *                               allowed non-trivial variations
 * @return true iff the defender can successfully defend
 */
boolean direct_defender_defend_in_n(slice_index si,
                                    stip_length_type n,
                                    unsigned int curr_max_nr_nontrivial);

/* Determine whether there is a defense after an attempted key move at
 * non-root level 
 * @param si slice index
 * @param n maximum number of half moves until end state has to be reached
 * @param curr_max_nr_nontrivial remaining maximum number of
 *                               allowed non-trivial variations
 * @return true iff the defender can successfully defend
 */
boolean direct_defender_can_defend_in_n(slice_index si,
                                        stip_length_type n,
                                        unsigned int curr_max_nr_nontrivial);

/* Solve postkey play play after the move that has just
 * been played in the current ply.
 * @param si slice index
 * @param n maximum number of half moves until goal
 * @return true iff >=1 solution was found
 */
boolean direct_defender_solve_postkey_in_n(slice_index si, stip_length_type n);

/* Solve postkey play play after the move that has just
 * been played at root level
 * @param refutations table containing refutations to move just played
 * @param si slice index
 */
void direct_defender_root_solve_postkey(table refutations, slice_index si);

#endif
