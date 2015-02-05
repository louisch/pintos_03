/* Utility methods to interact with the MLFQS. */

#ifndef __THREADS_MLFQS_H
#define __THREADS_MLFQS_H

#include "threads/thread.h"

fixed_point get_load_avg (void);
void mlfqs_thread_tick (struct list *ready_list);

#endif
