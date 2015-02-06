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

typedef struct
{
  int highest_priority;
  struct list *ready_list;
} rebuild_ready_list_struct;

typedef struct
{
  struct thread *t;
  struct list_elem elem;
} thread_list_elem;

static void mlfqs_ready_threads_init (void);
static inline fixed_point num_of_active_threads (struct list *ready_list);
static void mlfqs_update_priority (struct thread *t, void *aux UNUSED);
static void mlfqs_update_recent_cpu (struct thread *t, void *aux UNUSED);
static fixed_point mlfqs_new_load_avg (fixed_point old_load_avg, struct list *ready_list);

void
mlfqs_init (void)
{
  mlfqs_ready_threads_init ();
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
                  &(ready_thread->mlfqs_elem));
}

/* The actions to take during a thread_tick when -mlfqs is enabled */
void
mlfqs_thread_tick (struct list *ready_list)
{
  /* Updates load_avg, and the recent_cpu of all threads, when the system tick
     counter reaches a multiple of a second.
     This must happen at this time due to assumptions made by the tests.*/
  if (timer_ticks () % TIMER_FREQ == 0)
    {
      load_avg = mlfqs_new_load_avg (load_avg, ready_list);

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
      mlfqs_ready_threads_init ();
      thread_foreach (mlfqs_update_priority, NULL);
    }

  intr_yield_on_return();
}

static void
mlfqs_ready_threads_init (void)
{
  int i = 0;
  while (i < PRI_NUM)
    {
      list_init (&ready_array[i]);
      i++;
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
   niceness.

   If a ready thread is found, it will be added to ready_array, at the list at
   index equal to its priority.

   Note that there is nothing stopping one thread from being added twice to the
   array if this method is called multiple times without clearing the array in
   between. */
static void
mlfqs_update_priority (struct thread *t, void *aux UNUSED)
{
  fixed_point recent_cpu = fixed_point_dividei (t->recent_cpu, 4);
  int niceness = t->nice * 2;
  t->priority =
    to_integer_truncated (
      fixed_point_subtracti (fixed_point_subtract (PRI_MAX, recent_cpu),
                             niceness));

  if (t->status == THREAD_READY)
    {
      mlfqs_add_ready_thread(t);
    }
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

static fixed_point
mlfqs_new_load_avg (fixed_point old_load_avg, struct list *ready_list)
{
  fixed_point load_avg_factor =
    fixed_point_dividei (to_fixed_point (59), 60);
  fixed_point new_load_avg =
    fixed_point_add (fixed_point_multiply (old_load_avg, load_avg_factor),
                     fixed_point_dividei (num_of_active_threads (ready_list), 60));
  return new_load_avg;
}
