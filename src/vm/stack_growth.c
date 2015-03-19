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
  supp_page_create_segment (&thread_current ()->supp_page_table,
                            maximum_stack_addr (), WRITABLE,
                            STACK_SIZE);
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
is_valid_stack_access (void *fault_addr, void *esp)
{
  unsigned divergence = fault_addr > esp ?
    (unsigned)fault_addr - (unsigned)esp :
    (unsigned)esp - (unsigned)fault_addr;
  return divergence < MAX_ESP_DIVERGENCE;
}

/* The address of the page at the bottom-most possible position in the stack. */
uint8_t *
maximum_stack_addr (void)
{
  return (uint8_t *)PHYS_BASE - STACK_SIZE;
}
