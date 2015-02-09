/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
*/

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
     decrement it.

   - up or "V": increment the value (and wake up one waiting
     thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) 
{
  ASSERT (sema != NULL);

  sema->value = value;
  list_init (&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. */
void
sema_down (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  while (sema->value == 0) 
    {
      list_insert_ordered (&sema->waiters, &thread_current ()->elem,
                           &thread_priority_lt, NULL);
      if (thread_current ()->type == NONE)
        {
          thread_current ()->blocker = (void *) sema;
          thread_current ()->type = SEMA;
        }
      thread_block ();
    }
  sema->value--;
  intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) 
{
  enum intr_level old_level;
  bool success;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (sema->value > 0) 
    {
      sema->value--;
      success = true; 
    }
  else
    success = false;
  intr_set_level (old_level);

  return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  sema->value++;
  if (!list_empty (&sema->waiters))
    {
      struct thread *t = list_entry (list_pop_front (&sema->waiters),
                                     struct thread, elem);
      thread_unblock (t);
    }
  intr_set_level (old_level);
}

static int sema_get_priority (struct semaphore *sema);

/* Method for extracting the priority of the
   top thread in the semaphore's waiting list. */
static int
sema_get_priority (struct semaphore *sema)
{
  if (list_empty (&sema->waiters))
    {
      return 0;
    }
  else
    {
      struct thread *best_thread =
            list_entry (list_begin (&sema->waiters), struct thread, elem);
      return thread_get_priority_of (best_thread);
    }
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) 
{
  struct semaphore sema[2];
  int i;

  printf ("Testing semaphores...");
  sema_init (&sema[0], 0);
  sema_init (&sema[1], 0);
  thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
  for (i = 0; i < 10; i++) 
    {
      sema_up (&sema[0]);
      sema_down (&sema[1]);
    }
  printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) 
{
  struct semaphore *sema = sema_;
  int i;

  for (i = 0; i < 10; i++) 
    {
      sema_down (&sema[0]);
      sema_up (&sema[1]);
    }
}


static void lock_evaluate_priority (struct lock *lock);
static bool lock_try_increase_priority (struct lock *lock, int p);
static void lock_try_decrease_priority (struct lock *lock);
/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock)
{
  ASSERT (lock != NULL);

  lock->holder = NULL;
  lock->priority = PRI_MIN;
  sema_init (&lock->semaphore, 1);
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   Upon acquisition, the lock is added to the list of locks that
   the thread acquired.

   If the function fails to acquire the lock, its priority is
   donated and it is given a pointer to the lock.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
lock_acquire (struct lock *lock)
{
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (!lock_held_by_current_thread (lock));

  enum intr_level old_level;
  old_level = intr_disable ();
  if (!lock_try_acquire (lock)) /* This downs the semaphore if it succeeds. */
  {
    /* Lock acquisition failure. */
    /* Tell thread it is now under lock. */
    thread_current ()->blocker = (void *) lock;
    thread_current ()->type = LOCK;
    if (lock_try_increase_priority (lock, thread_get_priority ()))
      {
        thread_reinsert_lock (lock->holder, lock);
      }

    sema_down (&lock->semaphore);
    /* Thread now acquires the lock. */
    lock->holder = thread_current ();
    thread_add_acquired_lock (lock);
  }
  intr_set_level (old_level);
}

/* Tries to acquire LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock)
{
  bool success;

  ASSERT (lock != NULL);
  ASSERT (!lock_held_by_current_thread (lock));

  enum intr_level old_level = intr_disable ();
  success = sema_try_down (&lock->semaphore);
  if (success)
    {
      lock->holder = thread_current ();
      thread_add_acquired_lock (lock);
    }
  intr_set_level (old_level);
  return success;
}

/* Returns the cached priority of the lock. */
int
lock_get_priority_of (struct lock *lock)
{
  return lock->priority;
}

/* Gets the priority of the most important thread waiting on the lock. */
static void
lock_evaluate_priority (struct lock *lock)
{
  lock->priority = sema_get_priority (&lock->semaphore);
}

/* Set lock's priority to p if it is higher than its current priority. */
static bool
lock_try_increase_priority (struct lock *lock, int p)
{
  if (lock->priority < p)
  {
    lock->priority = p;
    return true;
  }
  return false;
}

/* Decreases priority value to the next priority in the list, if applicable. */
static void
lock_try_decrease_priority (struct lock *lock)
{
  struct list *waiters = &lock->semaphore.waiters;
  if (list_size (waiters) <= 1)
    {
      lock->priority = PRI_MIN;
    }
  else
    {
      struct thread *next_best_thread =
        list_entry (list_begin (waiters)->next, struct thread, elem);
      lock->priority = thread_get_priority_of (next_best_thread);
    }
}

/* Re-inserts thread t into the waiting list, keeping it ordered. */
void
lock_reinsert_thread (struct lock *lock, struct thread *t)
{
  struct list *waiters = &(lock->semaphore).waiters;
  list_remove (&t->elem);
  list_insert_ordered (waiters, &t->elem, &thread_priority_lt, NULL);

  lock_evaluate_priority (lock);
  thread_reinsert_lock (lock->holder, lock);
}

/* Releases LOCK, which must be owned by the current thread.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock) 
{
  ASSERT (lock != NULL);

  ASSERT (lock_held_by_current_thread (lock));

  enum intr_level old_level = intr_disable ();

  lock->holder = NULL;
  list_remove (&lock->elem);
  lock_try_decrease_priority (lock);
  sema_up (&lock->semaphore);
  
  intr_set_level (old_level);

}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) 
{
  ASSERT (lock != NULL);

  return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem 
  {
    struct list_elem elem;              /* List element. */
    struct semaphore semaphore;         /* This semaphore. */
  };

/* Function for comparing the priority of the threads in the
   waiting list to the priotity of the thread put to wait. */
bool
cond_sema_insert_priority (const struct list_elem *a,
                           const struct list_elem *b,
                           void *aux);

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond)
{
  ASSERT (cond != NULL);

  list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock) 
{
  struct semaphore_elem waiter;

  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));
  
  sema_init (&waiter.semaphore, 0);
  thread_current ()->blocker = (void *) cond;
  thread_current ()->type = COND;

  int p = thread_get_priority_of (thread_current ());
  list_insert_ordered (&cond->waiters, &waiter.elem,
                       &cond_sema_insert_priority, &p);
  lock_release (lock);
  sema_down (&waiter.semaphore);
  lock_acquire (lock);
}

/* Resorts waiting list in cond according to new priorities. */
void
cond_update (struct condition *cond)
{
  list_sort (&cond->waiters, &cond_sema_insert_priority, NULL);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));

  if (!list_empty (&cond->waiters)) 
    sema_up (&list_entry (list_pop_front (&cond->waiters),
                          struct semaphore_elem, elem)->semaphore);
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);

  while (!list_empty (&cond->waiters))
    cond_signal (cond, lock);
}

/* If given aux, uses value from aux for pa. */
bool
cond_sema_insert_priority (const struct list_elem *a UNUSED,
                           const struct list_elem *b,
                           void *aux)
{
  int pa;
  if (aux != NULL)
    {
      pa = *(int *) aux;
    }
  else
    {
      pa = sema_get_priority (&list_entry (a, struct semaphore_elem, elem)
                                              ->semaphore);
    }
  int pb = sema_get_priority (&list_entry (b, struct semaphore_elem, elem)
                                            ->semaphore);
  return pa > pb;
}
