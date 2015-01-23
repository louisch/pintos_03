#include <list.h>
#include "threads/synch.h"
#include <locked_list.h>

void
locked_list_push_back(struct locked_list *locked_list, struct list_elem *list_elem)
{
  lock_acquire(&(locked_list->lock));
  list_push_back(&(locked_list->list), list_elem);  
  lock_release(&(locked_list->lock));
}
