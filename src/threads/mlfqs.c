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

/* Estimates the average number of threads ready to run over the
   past minute. */
static fixed_point load_avg = 0;

static struct list ready_array[PRI_NUM];

static void mlfqs_ready_array_init (void);
static int num_of_threads_in_ready_array (void);
static inline fixed_point num_of_active_threads (void);
static void mlfqs_update_priority (struct thread *t, void *aux UNUSED);
static void mlfqs_update_recent_cpu (struct thread *t, void *aux UNUSED);
static fixed_point mlfqs_new_load_avg (fixed_point old_load_avg);

void
mlfqs_init (void)
{
  mlfqs_ready_array_init ();
}

fixed_point
get_load_avg (void)
{
  return load_avg;
}

/* Adds a thread with THREAD_RUNNING status to the MLFQS */
void
mlfqs_add_ready_thread (struct thread *ready_thread)
{
  ASSERT(ready_thread->status == THREAD_READY);
  ASSERT(ready_thread->priority >= PRI_MIN);
  ASSERT(ready_thread->priority <= PRI_MAX);

  list_push_back (&ready_array[ready_thread->priority],
                  &ready_thread->mlfqs_elem);
}

/* The actions to take during a thread_tick when -mlfqs is enabled */
void
mlfqs_thread_tick (void)
{
  /* Updates load_avg, and the recent_cpu of all threads, when the system tick
     counter reaches a multiple of a second.
     This must happen at this time due to assumptions made by the tests.*/
  if (timer_ticks () % TIMER_FREQ == 0)
    {
      load_avg = mlfqs_new_load_avg (load_avg);

      thread_foreach (mlfqs_update_recent_cpu, NULL);
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
    }

  intr_yield_on_return ();
}

/* Pops off the next thread to run from the ready_array.
   Returns the given default_thread if no thread can be found in the
   ready_array.*/
struct thread *
mlfqs_pop_next_thread_to_run (struct thread *default_thread)
{
  int i = PRI_MAX;
  while (i >= PRI_MIN)
    {
      if (!list_empty (&ready_array[i]))
        {
          return list_entry
            (list_pop_front (&ready_array[i]), struct thread, mlfqs_elem);
        }
      i--;
    }
  return default_thread;
}

/* Initialize the ready_array. */
static void
mlfqs_ready_array_init (void)
{
  int i = 0;
  while (i < PRI_NUM)
    {
      list_init (&ready_array[i]);
      i++;
    }
}

/* Calculates and returns the number of threads in ready_array. */
static int
num_of_threads_in_ready_array (void)
{
  int size_of_ready_list = 0;
  int i = PRI_MIN;
  while (i <= PRI_MAX)
    {
      size_of_ready_list += list_size (&ready_array[i]);
      i++;
    }

  return size_of_ready_list;
}

/* Calculates and returns the number of active threads.
   This is the current thread if it is not idle, plus the number of threads
   added into ready_array. */
static inline fixed_point
num_of_active_threads (void)
{
  int result = (is_idle (thread_current ()) ? 0 : 1) +
    num_of_threads_in_ready_array ();
  return to_fixed_point(result);
}

/* Updates the priority for a specific thread, based on recent_cpu and
   niceness.

   If a ready thread is found, it will be added to ready_array, at the list at
   index equal to its priority.

   Note that there is nothing stopping one thread from being added twice to the
   array if this method is called multiple times without clearing the array in
   between.

   The new priority is given by PRI_MAX - recent_cpu / 4 - nice * 2

   The new priority is kept within bounds by being set to PRI_MAX if it exceeds
   PRI_MAX, and being set to PRI_MIN if it is below PRI_MIN. */
static void
mlfqs_update_priority (struct thread *t, void *aux UNUSED)
{
  /* The factors of recent_cpu and nice (4 and 1/2 respectively) are slightly
     arbitary and were just found to work well. */
  fixed_point recent_cpu = fixed_point_dividei (t->recent_cpu, 4);
  int niceness = t->nice * 2;
  fixed_point pri_max_minus_recent_cpu =
    fixed_point_subtract (to_fixed_point (PRI_MAX), recent_cpu);
  int new_priority =
    to_integer_truncated (fixed_point_subtracti (pri_max_minus_recent_cpu,
                                                 niceness));
  if (new_priority < PRI_MIN)
    {
      new_priority = PRI_MIN;
    }
  if (new_priority > PRI_MAX)
    {
      new_priority = PRI_MAX;
    }

  if (new_priority != t->priority)
    {
      t->priority = new_priority;
      /* If a thread's status is THREAD_READY, it ought to have been inserted
         into the ready_array. */
      if (t->status == THREAD_READY)
        {
          list_remove (&t->mlfqs_elem);
          mlfqs_add_ready_thread (t);
        }
    }
}

/* A function that updates the recent_cpu of a thread.
   This matches the thread_action_func type, so can be used with
   thread_foreach.
   This is (2 * load_avg / 2 * load_avg + 1) * recent_cpu + nice
   Note how nice affects the decay of recent_cpu by adding a constant factor. */
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

/* Given the current load_avg, calculates the load_avg for the next update of
   load_avg.
   This is (59/60) * old_load_avg + num_of_active_threads() / 60 */
static fixed_point
mlfqs_new_load_avg (fixed_point old_load_avg)
{
  fixed_point load_avg_factor =
    fixed_point_dividei (to_fixed_point (59), 60);
  fixed_point new_load_avg =
    fixed_point_add (fixed_point_multiply (old_load_avg, load_avg_factor),
                     fixed_point_dividei (num_of_active_threads (), 60));
  return new_load_avg;
}
