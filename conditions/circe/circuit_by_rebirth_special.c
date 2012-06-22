#include "conditions/circe/circuit_by_rebirth_special.h"
#include "stipulation/stipulation.h"
#include "stipulation/pipe.h"
#include "pydata.h"
#include "debugging/trace.h"

#include <assert.h>

/* This module provides slice type STCirceCircuitSpecial
 */

/* Allocate a STCirceCircuitSpecial slice.
 * @return index of allocated slice
 */
slice_index alloc_circe_circuit_special_slice(void)
{
  slice_index result;

  TraceFunctionEntry(__func__);
  TraceFunctionParamListEnd();

  result = alloc_pipe(STCirceCircuitSpecial);

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

/* Try to solve in n half-moves after a defense.
 * @param si slice index
 * @param n maximum number of half moves until goal
 * @return length of solution found and written, i.e.:
 *            slack_length-2 defense has turned out to be illegal
 *            <=n length of shortest solution found
 *            n+2 no solution found
 */
stip_length_type circe_circuit_special_attack(slice_index si, stip_length_type n)
{
  stip_length_type result;
  square const sq_rebirth = sqrenais[nbply];

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u",n);
  TraceFunctionParamListEnd();

  if (sq_rebirth!=initsquare && GetPositionInDiagram(spec[sq_rebirth])==sq_rebirth)
    result = attack(slices[si].next1,n);
  else
    result = n+2;

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}
