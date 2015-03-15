#include "stack_growth.h"
#include <stdbool.h>
#include <threads/thread.h>
#include <threads/vaddr.h>
#include <vm/supp_page.h>

static void *stack_position_for (unsigned pages);

/* Initiate the stack growth system, adding entries to the supplementary page table. */
void
stack_growth_init (void)
{
  bool WRITABLE = true;
  uint8_t *entry_position = ((uint8_t *)PHYS_BASE - STACK_SIZE);
  while (entry_position != PHYS_BASE)
    {
      supp_page_create_entry (&thread_current ()->supp_page_table,
                              entry_position, WRITABLE);
      entry_position += PGSIZE;
    }
}

/* Heuristic for determining whether the stack should grow. */
bool
stack_should_grow (void *fault_addr, void *esp)
{
	return fault_addr > stack_position_for (MAX_PAGES_TO_ALLOC) &&
    esp > stack_position_for (MAX_PAGES_TO_ALLOC);
}

/* Grow the stack up to the given stack pointer.
   This does no checking, assuming that the conditions for growing
   the stack have been passed already. */
void
grow_stack (void *fault_addr, void *esp)
{
  struct thread *t = thread_current ();

  /* Get the entries up to the smaller of fault_addr and esp. */
  void *alloc_up_to = fault_addr < esp ? fault_addr : esp;
  unsigned num_to_allocate = ((t->mapped_stack_top - alloc_up_to) / PGSIZE) + 1;
  struct supp_page_entry *entry_buffer[num_to_allocate];
  supp_page_lookup_range (&t->supp_page_table, alloc_up_to,
                          entry_buffer, num_to_allocate);

  /* Map all the entries. */
  supp_page_map_entries (entry_buffer, num_to_allocate);
  t->mapped_stack_top += PGSIZE;
}

/* Returns an address to a position some pages into unallocated stack space.
   For example, if 3 pages have been allocated, then calling this function with
   an argument of 2 would return the address 5 pages down from PHYS_BASE. */
static void *
stack_position_for (unsigned pages)
{
  return thread_current ()->mapped_stack_top - (pages * PGSIZE);
}
