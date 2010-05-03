#include "pyflight.h"
#include "pydata.h"
#include "pypipe.h"
#include "stipulation/branch.h"
#include "stipulation/battle_play/defense_play.h"
#include "trace.h"

#include <assert.h>
#include <stdlib.h>

static unsigned int max_nr_flights;

/* Reset the max flights setting to off
 */
void reset_max_flights(void)
{
  max_nr_flights = INT_MAX;
}

/* Read the requested max flight setting from a text token entered by
 * the user
 * @param textToken text token from which to read
 * @return true iff max flight setting was successfully read
 */
boolean read_max_flights(const char *textToken)
{
  boolean result;
  char *end;
  unsigned long const requested_max_nr_flights = strtoul(textToken,&end,10);

  if (textToken!=end && requested_max_nr_flights<=nr_squares_on_board)
  {
    max_nr_flights = (unsigned int)requested_max_nr_flights;
    result = true;
  }
  else
    result = false;

  return result;
}

/* Retrieve the current max flights setting
 * @return current max flights setting
 *         UINT_MAX if max flights option is not active
 */
unsigned int get_max_flights(void)
{
  return max_nr_flights;
}

/* **************** Private helpers ***************
 */

/* Determine whether the defending side has more flights than allowed
 * by the user.
 * @param defender defending side
 * @return true iff the defending side has too many flights.
 */
static boolean has_too_many_flights(Side defender)
{
  boolean result;
  square const save_rbn = defender==Black ? rn : rb;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",defender);
  TraceFunctionParamListEnd();

  if (save_rbn==initsquare)
    result = false;
  else
  {
    unsigned int number_flights_left = max_nr_flights+1;

    genmove(defender);

    while (encore() && number_flights_left>0)
    {
      if (jouecoup(nbply,first_play))
      {
        square const rbn = defender==Black ? rn : rb;
        if (save_rbn!=rbn && !echecc(nbply,defender))
          --number_flights_left;
      }

      repcoup();
    }

    finply();

    result = number_flights_left==0;
  }


  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

/* **************** Initialisation ***************
 */

/* Initialise a STMaxFlightsquares slice
 * @param length maximum number of half moves until end of branch
 * @return identifier of allocated slice
 */
static slice_index alloc_maxflight_guard_slice(stip_length_type length)
{
  slice_index result;
  stip_length_type const parity = (length-slack_length_battle)%2;

  TraceFunctionEntry(__func__);
  TraceFunctionParamListEnd();

  result = alloc_branch(STMaxFlightsquares,length,slack_length_battle+parity);

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}


/* **************** Implementation of interface DirectDefender **********
 */

/* Try to defend after an attempted key move at root level
 * @param si slice index
 * @param n maximum number of half moves until end state has to be reached
 * @param n_min minimum number of half-moves of interesting variations
 *              (slack_length_battle <= n_min <= slices[si].u.branch.length)
 * @param max_nr_refutations how many refutations should we look for
 * @return <slack_length_battle - stalemate
 *         <=n solved  - return value is maximum number of moves
 *                       (incl. defense) needed
 *         n+2 refuted - <=max_nr_refutations refutations found
 *         n+4 refuted - >max_nr_refutations refutations found
 */
stip_length_type maxflight_guard_root_defend(slice_index si,
                                             stip_length_type n,
                                             stip_length_type n_min,
                                             unsigned int max_nr_refutations)
{
  stip_length_type result;
  Side const defender = slices[si].starter;
  slice_index const next = slices[si].u.pipe.next;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u",n);
  TraceFunctionParam("%u",n_min);
  TraceFunctionParam("%u",max_nr_refutations);
  TraceFunctionParamListEnd();

  if (n>slack_length_battle+3 && has_too_many_flights(defender))
    result = n+4;
  else
    result = defense_root_defend(next,n,n_min,max_nr_refutations);

  TraceFunctionExit(__func__);
  TraceValue("%u",result);
  TraceFunctionResultEnd();
  return result;
}

/* Try to defend after an attempted key move at non-root level.
 * When invoked with some n, the function assumes that the key doesn't
 * solve in less than n half moves.
 * @param si slice index
 * @param n maximum number of half moves until end state has to be reached
 * @param n_min minimum number of half-moves of interesting variations
 *              (slack_length_battle <= n_min <= slices[si].u.branch.length)
 * @return true iff the defender can defend
 */
boolean maxflight_guard_defend_in_n(slice_index si,
                                    stip_length_type n,
                                    stip_length_type n_min)
{
  Side const defender = slices[si].starter;
  slice_index const next = slices[si].u.pipe.next;
  boolean result;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u",n);
  TraceFunctionParam("%u",n_min);
  TraceFunctionParamListEnd();

  assert(n%2==slices[si].u.branch.length%2);

  if (n>slack_length_battle+3 && has_too_many_flights(defender))
    result = true;
  else
    result = defense_defend_in_n(next,n,n_min);

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

/* Determine whether there are refutations after an attempted key move
 * at non-root level
 * @param si slice index
 * @param n maximum number of half moves until end state has to be reached
 * @param n_min minimum number of half-moves of interesting variations
 *              (slack_length_battle <= n_min <= slices[si].u.branch.length)
 * @param max_nr_refutations how many refutations should we look for
 * @return <=n solved  - return value is maximum number of moves
 *                       (incl. defense) needed
 *         n+2 refuted - <=max_nr_refutations refutations found
 *         n+4 refuted - >max_nr_refutations refutations found
 */
stip_length_type
maxflight_guard_can_defend_in_n(slice_index si,
                                stip_length_type n,
                                stip_length_type n_min,
                                unsigned int max_nr_refutations)
{
  Side const defender = slices[si].starter;
  slice_index const next = slices[si].u.pipe.next;
  unsigned int result;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u",n);
  TraceFunctionParam("%u",n_min);
  TraceFunctionParam("%u",max_nr_refutations);
  TraceFunctionParamListEnd();

  if (n>slack_length_battle+3 && has_too_many_flights(defender))
    result = n+4;
  else
    result = defense_can_defend_in_n(next,n,n_min,max_nr_refutations);

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}


/* **************** Stipulation instrumentation ***************
 */

/* Insert a STMaxFlightsquares slice before each defender slice
 * @param si identifier defender slice
 * @param st address of structure representing the traversal
 */
static void maxflight_guard_inserter(slice_index si,stip_structure_traversal *st)
{
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  pipe_append(slices[si].prev,
              alloc_maxflight_guard_slice(slices[si].u.branch.length));
  stip_traverse_structure_children(si,st);

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

static stip_structure_visitor const maxflight_guards_inserters[] =
{
  &stip_traverse_structure_children,  /* STProxy */
  &stip_traverse_structure_children,  /* STAttackMove */
  &maxflight_guard_inserter, /* STDefenseMove */
  &stip_traverse_structure_children,  /* STHelpMove */
  &stip_traverse_structure_children,  /* STHelpFork */
  &stip_traverse_structure_children,  /* STSeriesMove */
  &stip_traverse_structure_children,  /* STSeriesFork */
  &stip_traverse_structure_children,  /* STLeafDirect */
  &stip_traverse_structure_children,  /* STLeafHelp */
  &stip_traverse_structure_children,  /* STLeafForced */
  &stip_traverse_structure_children,  /* STReciprocal */
  &stip_traverse_structure_children,  /* STQuodlibet */
  &stip_traverse_structure_children,  /* STNot */
  &stip_traverse_structure_children,  /* STMoveInverterRootSolvableFilter */
  &stip_traverse_structure_children,  /* STMoveInverterSolvableFilter */
  &stip_traverse_structure_children,  /* STMoveInverterSeriesFilter */
  &stip_traverse_structure_children,  /* STAttackRoot */
  &stip_traverse_structure_children,  /* STBattlePlaySolutionWriter */
  &stip_traverse_structure_children,  /* STPostKeyPlaySolutionWriter */
  &stip_traverse_structure_children,  /* STPostKeyPlaySuppressor */
  &stip_traverse_structure_children,  /* STContinuationWriter */
  &stip_traverse_structure_children,  /* STRefutationsWriter */
  &stip_traverse_structure_children,  /* STThreatWriter */
  &stip_traverse_structure_children,  /* STThreatEnforcer */
  &stip_traverse_structure_children,  /* STRefutationsCollector */
  &stip_traverse_structure_children,  /* STVariationWriter */
  &stip_traverse_structure_children,  /* STRefutingVariationWriter */
  &stip_traverse_structure_children,  /* STNoShortVariations */
  &stip_traverse_structure_children,  /* STAttackHashed */
  &stip_traverse_structure_children,  /* STHelpRoot */
  &stip_traverse_structure_children,  /* STHelpShortcut */
  &stip_traverse_structure_children,  /* STHelpHashed */
  &stip_traverse_structure_children,  /* STSeriesRoot */
  &stip_traverse_structure_children,  /* STSeriesShortcut */
  &stip_traverse_structure_children,  /* STParryFork */
  &stip_traverse_structure_children,  /* STSeriesHashed */
  &stip_traverse_structure_children,  /* STSelfCheckGuardRootSolvableFilter */
  &stip_traverse_structure_children,  /* STSelfCheckGuardSolvableFilter */
  &stip_traverse_structure_children,  /* STSelfCheckGuardRootDefenderFilter */
  &stip_traverse_structure_children,  /* STSelfCheckGuardAttackerFilter */
  &stip_traverse_structure_children,  /* STSelfCheckGuardDefenderFilter */
  &stip_traverse_structure_children,  /* STSelfCheckGuardHelpFilter */
  &stip_traverse_structure_children,  /* STSelfCheckGuardSeriesFilter */
  &stip_traverse_structure_children,  /* STDirectDefenderFilter */
  &stip_traverse_structure_children,  /* STReflexHelpFilter */
  &stip_traverse_structure_children,  /* STReflexSeriesFilter */
  &stip_traverse_structure_children,  /* STReflexRootSolvableFilter */
  &stip_traverse_structure_children,  /* STReflexAttackerFilter */
  &stip_traverse_structure_children,  /* STReflexDefenderFilter */
  &stip_traverse_structure_children,  /* STSelfDefense */
  &stip_traverse_structure_children,  /* STRestartGuardRootDefenderFilter */
  &stip_traverse_structure_children,  /* STRestartGuardHelpFilter */
  &stip_traverse_structure_children,  /* STRestartGuardSeriesFilter */
  &stip_traverse_structure_children,  /* STIntelligentHelpFilter */
  &stip_traverse_structure_children,  /* STIntelligentSeriesFilter */
  &stip_traverse_structure_children,  /* STGoalReachableGuardHelpFilter */
  &stip_traverse_structure_children,  /* STGoalReachableGuardSeriesFilter */
  &stip_traverse_structure_children,  /* STKeepMatingGuardRootDefenderFilter */
  &stip_traverse_structure_children,  /* STKeepMatingGuardAttackerFilter */
  &stip_traverse_structure_children,  /* STKeepMatingGuardDefenderFilter */
  &stip_traverse_structure_children,  /* STKeepMatingGuardHelpFilter */
  &stip_traverse_structure_children,  /* STKeepMatingGuardSeriesFilter */
  &stip_traverse_structure_children,  /* STMaxFlightsquares */
  &stip_traverse_structure_children,  /* STDegenerateTree */
  &stip_traverse_structure_children,  /* STMaxNrNonTrivial */
  &stip_traverse_structure_children,  /* STMaxNrNonTrivialCounter */
  &stip_traverse_structure_children,  /* STMaxThreatLength */
  &stip_traverse_structure_children,  /* STMaxTimeRootDefenderFilter */
  &stip_traverse_structure_children,  /* STMaxTimeDefenderFilter */
  &stip_traverse_structure_children,  /* STMaxTimeHelpFilter */
  &stip_traverse_structure_children,  /* STMaxTimeSeriesFilter */
  &stip_traverse_structure_children,  /* STMaxSolutionsRootSolvableFilter */
  &stip_traverse_structure_children,  /* STMaxSolutionsRootDefenderFilter */
  &stip_traverse_structure_children,  /* STMaxSolutionsHelpFilter */
  &stip_traverse_structure_children,  /* STMaxSolutionsSeriesFilter */
  &stip_traverse_structure_children,  /* STStopOnShortSolutionsRootSolvableFilter */
  &stip_traverse_structure_children,  /* STStopOnShortSolutionsHelpFilter */
  &stip_traverse_structure_children   /* STStopOnShortSolutionsSeriesFilter */
};

/* Instrument stipulation with STMaxFlightsquares slices
 */
void stip_insert_maxflight_guards(void)
{
  stip_structure_traversal st;

  TraceFunctionEntry(__func__);
  TraceFunctionParamListEnd();

  TraceStipulation(root_slice);

  stip_structure_traversal_init(&st,&maxflight_guards_inserters,0);
  stip_traverse_structure(root_slice,&st);

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}
