#include <list.h>
#include "threads/synch.h"
#include <locked_list.h>

/* Apply the getter function onto the list inside of locked_list but acquiring 
   and releasing the lock before and after. */
struct list_elem *
locked_list_get_elem(struct locked_list *locked_list, list_elem_getter getter)
{
  lock_acquire(&(locked_list->lock));
  struct list_elem *list_elem = getter(&(locked_list->list));
  lock_release(&(locked_list->lock));
  return list_elem;
}

/* */
void
locked_list_init(struct locked_list *locked_list){
  list_init(&(locked_list->list));
  lock_init(&(locked_list->lock));
}

void
locked_list_push_back(struct locked_list *locked_list, struct list_elem *list_elem)
{
  lock_acquire(&(locked_list->lock));
  list_push_back(&(locked_list->list), list_elem);  
  lock_release(&(locked_list->lock));
}

bool
locked_list_empty(struct locked_list* locked_list)
{
  lock_acquire(&(locked_list->lock));
  bool is_empty = list_empty(&(locked_list->list));
  lock_release(&(locked_list->lock));
  return is_empty;
}
