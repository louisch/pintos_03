#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <fixed_point.h>
#include <list.h>
#include <stdint.h>
#include "synch.h"
#ifdef USERPROG
#include <user/syscall.h>
#include "userprog/process.h"
#endif
#ifdef VM
#include <vm/supp_page.h>
#endif

/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };

/* Lists various structures that thread can be waiting on. */
enum blocker_type
  {
    NONE,      /* Thread is running or in ready list. */
    SEMA,      /* Thread is waiting on a semaphore. */
    LOCK,      /* Thread is waiting on a lock. */
    COND       /* Thread is waiting on a conditional. */
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */
#define PRI_NUM 64                      /* Number of priority states. */

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */

/* This include must be placed here because tid_t must be declared before
   including process.h */
#ifdef USERPROG
#include "userprog/process.h"
#endif
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority of thread. */

    int nice;                           /* Niceness. */
    fixed_point recent_cpu;             /* CPU time this received 'recently' */

    struct list_elem allelem;           /* List element for all threads list */

    /* Added for priority sorting and donations. */
    struct list locks;                  /* Locks acquired by the thread. */
    void *blocker;                      /* Struct that blocks this thread.*/
    enum blocker_type type;             /* What is blocking the thread? */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

    struct list_elem mlfqs_elem;        /* Used by the MLFQS */

#ifdef USERPROG
    /* The pid of the process owning this thread. */
    process_info p_info;
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
#endif
#ifdef VM
    struct supp_page_table supp_page_table; /* Supplementary Page Table. */
    void *stack_bottom; /* Address of page at bottom of allocated stack. */
#endif

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
  };

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

/* Predeclarations of structs from process.h, which needs to include this
   file, but will not then recursively include process.h again. */
typedef struct process_info process_info;
typedef struct persistent_info persistent_info;

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);
persistent_info *thread_create_thread (const char *name, int priority,
                            thread_func *function, void *aux);

void thread_give_way (struct thread *t);
void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

void thread_set_priority (int);
void thread_set_priority_of (struct thread *, int);

void thread_add_acquired_lock (struct lock *);
void thread_reinsert_lock (struct thread *, struct lock *);

int thread_get_priority (void);
int thread_get_priority_of (struct thread *);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

bool is_idle (const struct thread *t);

bool thread_priority_lt (const struct list_elem *a,
                         const struct list_elem *b,
                         void *aux UNUSED);

bool lock_list_elem_lt (const struct list_elem *a,
                        const struct list_elem *b,
                        void *aux UNUSED);

#endif /* threads/thread.h */
