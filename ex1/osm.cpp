#include <iostream>
#include "osm.h"
#include <sys/time.h>



/* Time measurement function for a simple arithmetic operation.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_operation_time (unsigned int iterations)
{
  if (iterations == 0)
    {
      return - 1;
    }
  int x;
  double sum = 0;
  struct timeval start_time{};
  struct timeval end_time{};
  for (unsigned int i = 0; i < iterations; i += 5)
    {
      gettimeofday (&start_time, nullptr);
      x = x + 7;
      x = x - 7;
      x = x + 7;
      x = x - 7;
      x = x + 7;
      gettimeofday (&end_time, nullptr);
      sum += (end_time.tv_sec - start_time.tv_sec)*1000000000 + (end_time.tv_usec - start_time.tv_usec)*1000;
    }
  return (sum / iterations);
}

/**
 * empty function call
 */
void empty_func (){}

/* Time measurement function for an empty function call.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_function_time (unsigned int iterations)
{
  if (iterations == 0)
    {
      return - 1;
    }
  double sum = 0;
  struct timeval start_time{};
  struct timeval end_time{};
  for (unsigned int i = 0; i < iterations; i += 5)
    {
      gettimeofday (&start_time, nullptr);
      empty_func ();
      empty_func ();
      empty_func ();
      empty_func ();
      empty_func ();
      gettimeofday (&end_time, nullptr);
      sum += (end_time.tv_sec - start_time.tv_sec)*1000000000 + (end_time.tv_usec - start_time.tv_usec)*1000;
    }
  return (sum / iterations);
}

/* Time measurement function for an empty trap into the operating system.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_syscall_time (unsigned int iterations)
{
  if (iterations == 0)
    {
      return - 1;
    }
  double sum = 0;
  struct timeval start_time{};
  struct timeval end_time{};
  for (unsigned int i = 0; i < iterations; i += 5)
    {
      gettimeofday (&start_time, nullptr);
      OSM_NULLSYSCALL;
      OSM_NULLSYSCALL;
      OSM_NULLSYSCALL;
      OSM_NULLSYSCALL;
      OSM_NULLSYSCALL;
      gettimeofday (&end_time, nullptr);
      sum += (end_time.tv_sec - start_time.tv_sec)*1000000000 + (end_time.tv_usec - start_time.tv_usec)*1000;
    }
  return (sum / iterations);
}


