#ifndef __LIB_KERNEL_LOCKED_LIST_H
#define __LIB_KERNEL_LOCKED_LIST_H

#include <list.h>
#include "threads/synch.h"

/* Locked list - Provides a synchronisd verision of the list.
   All operations on the locked list acquire and release the lock
   before and after each operation. 

   
   However int intialization the lock does not acquire and release as it
   neither has been initialised yet. */
struct locked_list
  {
    struct list list;
    struct lock lock;
  };

/* Locked list function counter parts. */
typedef struct list_elem *(*list_elem_getter)(struct list *);

/* Takes a getter function and applies it to the list inside locked_list. */
struct list_elem *locked_list_get_elem(struct locked_list *locked_list,
                                     list_elem_getter getter);

void locked_list_init(struct locked_list *locked_list);
void locked_list_push_back(struct locked_list *locked_list,
                           struct list_elem *list_elem);
bool locked_list_empty(struct locked_list* locked_list);

#endif