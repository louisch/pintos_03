#include "threads/mlfqs.h"
#include <debug.h>
#include <list.h>

#include <stddef.h>
#include "devices/timer.h"
#include "threads/interrupt.h"
#include "threads/thread.h"

/* The number of ticks to wait until updating priority in the MLFQS
   This is only relevant when -mlfqs is set. */
#define MLFQS_PRIORITY_UPDATE_FREQ 4

static inline fixed_point num_of_active_threads (struct list *ready_list);
static void mlfqs_update_priority (struct thread *t, void *aux UNUSED);
static void mlfqs_update_recent_cpu (struct thread *t, void *aux UNUSED);

/* Estimates the average number of threads ready to run over the
   past minute. */
static fixed_point load_avg = {0};

fixed_point
get_load_avg (void)
{
  return load_avg;
}

/* The actions to take during a thread_tick when -mlfqs is enabled */
void
mlfqs_thread_tick (struct list *ready_list)
{

  if(thread_current()->priority ==
     list_entry (list_begin (ready_list), struct thread, elem)->priority)
    {
      intr_yield_on_return();
    }

  /* Increment the current thread's recent_cpu */
  if (!is_idle (thread_current ()))
    {
      thread_current ()->recent_cpu =
        fixed_point_addi (thread_current ()->recent_cpu, 1);
    }

  /* Update all threads' priorities. */
  if (timer_ticks () % MLFQS_PRIORITY_UPDATE_FREQ == 0)
    {
      thread_foreach (mlfqs_update_priority, NULL);
      list_sort (ready_list, priority_less_than, NULL);
    }

  /* Updates load_avg, and the recent_cpu of all threads, when the system tick
     counter reaches a multiple of a second.
     This must happen at this time due to assumptions made by the tests.*/
  if (timer_ticks () % TIMER_FREQ == 0)
    {
      thread_foreach (mlfqs_update_recent_cpu, NULL);

      fixed_point load_avg_factor =
        fixed_point_dividei (to_fixed_point (59), 60);
      load_avg =
        fixed_point_add (fixed_point_multiply (load_avg, load_avg_factor),
                         fixed_point_dividei (num_of_active_threads (ready_list), 60));
    }
}

/* Calculates and returns the number of active threads.
   This is the current thread if it is not idle, and the number of threads with
   the ready status. */
static inline fixed_point
num_of_active_threads (struct list *ready_list)
{
  int result = (is_idle (thread_current ()) ? 0 : 1) +
    list_size (ready_list);
  return to_fixed_point(result);
}

/* Updates the priority for a specific thread, based on recent_cpu and
   niceness. */
static void
mlfqs_update_priority (struct thread *t, void *aux UNUSED)
{
  int recent_cpu = to_integer_truncated (fixed_point_dividei (t->recent_cpu, 4));
  int niceness = t->nice * 2;
  t->priority = PRI_MAX - recent_cpu - niceness;
}

/* A function that updates the recent_cpu of a thread.
   This matches the thread_action_func type, so can be used with
   thread_foreach. */
static void
mlfqs_update_recent_cpu (struct thread *t, void *aux UNUSED)
{
  fixed_point double_load_avg = fixed_point_multiplyi (load_avg, 2);
  fixed_point coefficient =
    fixed_point_divide (double_load_avg,
                        fixed_point_addi (double_load_avg, 1));
  fixed_point new_recent_cpu =
    fixed_point_addi (fixed_point_multiply (coefficient, t->recent_cpu),
                      t->nice);
  t->recent_cpu = new_recent_cpu;
}
