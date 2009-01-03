#if !defined(PYQUODLI_H)
#define PYQUODLI_H

#include "py.h"
#include "pystip.h"
#include "boolean.h"

/* This module provides functionality dealing with quodlibet
 * (i.e. logical OR) stipulation slices.
 */


/* Detect a priori unsolvability of a slice (e.g. because of forced
 * reflex mates)
 * @param si slice index
 * @return true iff slice is a priori unsolvable
 */
boolean quodlibet_end_is_unsolvable(slice_index si);

/* Write a priori unsolvability (if any) of a slice (e.g. forced
 * reflex mates).
 * Assumes slice_is_unsolvable(si)
 * @param si slice index
 */
void quodlibet_write_unsolvability(slice_index si);

/* Determine and write continuations at end of quodlibet slice
 * @param continuations table where to store continuing moves
 *                      (e.g. threats)
 * @param si index of quodlibet slice
 */
void quodlibet_end_solve_continuations(int continuations, slice_index si);

/* Find and write set play
 * @param si slice index
 */
void quodlibet_root_end_solve_setplay(slice_index si);

/* Find and write defender's set play in self/reflex play if every
 * set move leads to end
 * @param si slice index
 * @return true iff every defender's move leads to end
 */
boolean d_quodlibet_root_end_solve_complete_set(slice_index si);

/* Write the key just played, then solve the post key play (threats,
 * variations), starting at the end of a quodlibet slice.
 * @param si slice index
 * @param type type of attack
 */
void d_quodlibet_root_end_write_key_solve_postkey(slice_index si,
                                                  attack_type type);

/* Find and write variations from the end of a quodlibet slice.
 * @param si slice index
 */
void d_quodlibet_end_solve_variations(slice_index si);

/* Determine whether the attacker wins at the end of a quodlibet slice
 * @param si slice index of leaf slice
 * @return true iff attacker wins
 */
boolean d_quodlibet_end_does_attacker_win(slice_index si);

/* Determine whether the defender has directly lost in direct play
 * with his move just played.
 * Assumes that there is no short win for the defending side.
 * @param si slice identifier
 * @return true iff the defending side has directly lost
 */
boolean d_quodlibet_end_has_defender_lost(slice_index si);

/* Determine whether the defender has immediately won in direct play
 * with his move just played.
 * @param si slice identifier
 * @return true iff the defending side has directly won
 */
boolean d_quodlibet_end_has_defender_won(slice_index si);

/* Determine whether the attacker has immediately won in direct play
 * with his move just played.
 * @param defender defending side
 * @param si slice identifier
 * @return true iff the attacking side has directly won
 */
boolean d_quodlibet_end_has_attacker_won(slice_index si);

/* Determine whether the attacker has immediately lost in direct play
 * with his move just played.
 * @param si slice identifier
 * @return true iff the attacking side has directly lost
 */
boolean d_quodlibet_end_has_attacker_lost(slice_index si);

/* Has the threat just played been refuted by the preceding defense?
 * @param si identifies stipulation slice
 * @return true iff the threat is refuted
 */
boolean d_quodlibet_end_is_threat_refuted(slice_index si);

/* Continue solving at the end of a quodlibet slice
 * @param si slice index
 * @return true iff >=1 solution was found
 */
boolean quodlibet_end_solve(slice_index si);

/* Solve at root level at the end of a quodlibet slice
 * @param restartenabled true iff option movenum is activated
 * @param si slice index
 * @return true iff >=1 solution was found
 */
boolean quodlibet_root_end_solve(boolean restartenabled, slice_index si);

/* Detect starter field with the starting side if possible. 
 * @param si identifies slice
 * @param is_duplex is this for duplex?
 */
void quodlibet_detect_starter(slice_index si, boolean is_duplex);

/* Impose the starting side on a slice.
 * @param si identifies sequence
 * @param s starting side of slice
 */
void quodlibet_impose_starter(slice_index si, Side s);

#endif
