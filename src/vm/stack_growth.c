#include "stack_growth.h"
#include <stdbool.h>
#include <threads/thread.h>
#include <threads/vaddr.h>
#include <vm/supp_page.h>

static void *next_alloc_position (unsigned pages_to_alloc);

#define MAX_PAGES_TO_ALLOC 1

bool
stack_should_grow (void *esp, void *fault_addr)
{
	return esp == fault_addr &&
    fault_addr < PHYS_BASE && 
    fault_addr > PHYS_BASE - STACK_SIZE && 
    fault_addr > next_alloc_position (MAX_PAGES_TO_ALLOC);
}

void
grow_stack (void *esp)
{
  bool WRITABLE = true;
  struct thread *t = thread_current ();

  while (next_alloc_position (1) - PGSIZE < esp)
    {
      if (next_alloc_position (1) < PHYS_BASE - STACK_SIZE)
        {
          thread_exit ();
        }
      struct supp_page_entry *entry =
        supp_page_create_entry (&t->supp_page_table, 
                                next_alloc_position (1), WRITABLE);
      /* Create the page right now instead of waiting for it to fault,
         as some kernel code needs it set up anyway. */
      supp_page_map_entry (entry);

      t->num_pages_in_stack++;
    }
}

static void *
next_alloc_position (unsigned pages_to_alloc)
{
  return PHYS_BASE - (thread_current ()->num_pages_in_stack + pages_to_alloc) * PGSIZE;
}