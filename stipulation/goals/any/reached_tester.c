#include "stipulation/goals/any/reached_tester.h"
#include "stipulation/pipe.h"
#include "stipulation/goals/reached_tester.h"
#include "stipulation/boolean/true.h"
#include "debugging/trace.h"

/* This module provides functionality dealing with slices that detect
 * whether an any goal has just been reached
 */

/* Allocate a system of slices that tests whether any has been reached
 * @return index of entry slice
 */
slice_index alloc_goal_any_reached_tester_system(void)
{
  slice_index result;
  slice_index any_tester;
  Goal const goal = { goal_any, initsquare };

  TraceFunctionEntry(__func__);
  TraceFunctionParamListEnd();

  any_tester = alloc_pipe(STGoalAnyReachedTester);
  pipe_link(any_tester,alloc_true_slice());
  result = alloc_goal_reached_tester_slice(goal,any_tester);

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}
