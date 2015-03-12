#ifndef VM_SUPP_PAGE_H
#define VM_SUPP_PAGE_H

#include <stdbool.h>
#include <lib/kernel/hash.h>

/* The supplementary page table keeps track of additional information on each
   page that the page table (in pagedir.h) cannot.

   This is used during page faults, to find out where the page for a virtual
   user address actually is. Pages may have been swapped out into swap space, or
   they may simply be in a file.  Alternatively, they may already be in memory,
   just not mapped in the page table.  In any case, the page must be placed into
   a frame if not already, and then the virtual user address must be mapped to
   the frame in the page table.

   This supplementary page table provides the data for that functionality. */
struct supp_page_table
{
  struct hash table; /* The internal hash table. */
};

/* An entry in the supplementary page table.

   This is where the data for each page in the supplementary page table is
   stored. */
struct supp_page_entry
{
  struct hash_elem supp_elem; /* For placing this into a supp_page_table. */
  void *uaddr; /* The virtual user address of this page. */
  bool writable; /* Whether this page is writable or not. */
  bool all_zeroes; /* Whether this page is filled with zeroes. */
};

void supp_page_table_init (struct supp_page_table *supp_page_table);
struct supp_page_entry *create_entry (struct supp_page_table *supp_page_table,
                                      void *uaddr, bool writable, bool all_zeroes);
void *map_user_addr (uint32_t *pd, struct supp_page_entry *entry);

#endif
