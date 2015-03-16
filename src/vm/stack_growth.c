#include "stack_growth.h"
#include <stdbool.h>
#include <threads/thread.h>
#include <threads/vaddr.h>
#include <vm/supp_page.h>

/* Initiate the stack growth system, adding entries to the supplementary page table. */
void
stack_growth_init (void)
{
  bool WRITABLE = true;
  uint8_t *entry_position = maximum_stack_addr ();
  while (entry_position != PHYS_BASE)
    {
      supp_page_create_entry (&thread_current ()->supp_page_table,
                              entry_position, WRITABLE);
      entry_position += PGSIZE;
    }
}

/* Returns whether the given address is within the stack.
   This stack access may be invalid though, which stack_should_grow can check
   for. */
bool
is_stack_access (void *fault_addr)
{
  return maximum_stack_addr () < (uint8_t *)fault_addr &&
    fault_addr < PHYS_BASE;
}

/* Heuristic for determining whether the stack should grow. */
bool
stack_should_grow (void *fault_addr, void *esp)
{
  unsigned divergence = fault_addr > esp ?
    (unsigned)fault_addr - (unsigned)esp :
    (unsigned)esp - (unsigned)fault_addr;
  return divergence < MAX_ESP_DIVERGENCE;
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

uint8_t *
maximum_stack_addr (void)
{
  return (uint8_t *)PHYS_BASE - STACK_SIZE;
}
