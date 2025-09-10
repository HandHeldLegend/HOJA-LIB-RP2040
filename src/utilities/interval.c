#include "utilities/interval.h"

// This is a static inline function that will return
// a value of 'true' if an interval is met.
// IE helps you run functions at set intervals without blocking (as much).
bool interval_run(uint64_t timestamp, uint32_t interval, interval_s *state)
{
  state->this_time = timestamp;

  // Clear variable
  uint64_t diff = 0;

  diff = state->this_time - state->last_time;
  // We want a target rate according to our variable
  if (diff >= interval)
  {
    // Set the last time
    state->last_time = state->this_time;
    return true;
  }
  return false;
}

// This is a function that will return
// offers the ability to restart the timer
bool interval_resettable_run(uint64_t timestamp, uint32_t interval, bool reset, interval_s *state)
{

  state->this_time = timestamp;

  if (reset)
  {
    state->last_time = state->this_time;
    return false;
  }

  // Clear variable
  uint64_t diff = 0;

  diff = state->this_time - state->last_time;
  // We want a target rate according to our variable
  if (diff >= interval)
  {
    // Set the last time
    state->last_time = state->this_time;
    return true;
  }
  return false;
}