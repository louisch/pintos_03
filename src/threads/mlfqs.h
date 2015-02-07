/* Utility methods to interact with the MLFQS. */

#ifndef __THREADS_MLFQS_H
#define __THREADS_MLFQS_H

#include "threads/thread.h"

void mlfqs_init (void);
void mlfqs_add_ready_thread (struct thread *ready_thread);
fixed_point get_load_avg (void);
void mlfqs_thread_tick (void);

#endif
