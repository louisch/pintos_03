#ifndef VM_SUPP_PAGE_H
#define VM_SUPP_PAGE_H

#include <stdbool.h>
#include <lib/kernel/hash.h>

#include <filesys/file.h>
#include <filesys/off_t.h>

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
   stored.

   Pages may have originated from a file (they are read from a file into
   memory). If they did not, then the file field will be set to NULL. */
struct supp_page_entry
{
  struct hash_elem supp_elem; /* For placing this into a supp_page_table. */
  void *uaddr; /* The virtual user address of this page. */

  /* File data fields */
  struct file *file; /* The file that this page is read from. NULL if not a file. */
  off_t offset; /* Offset amount into the file that this page starts at. */
  /* We will read PAGE_READ_BYTES bytes from FILE
     and zero the final PAGE_ZERO_BYTES bytes. */
  size_t page_read_bytes;
  size_t page_zero_bytes;

  /* Other properties */
  bool writable; /* Whether this page is writable or not. */
};

void supp_page_table_init (struct supp_page_table *supp_page_table);
struct supp_page_entry *supp_page_create_entry (struct supp_page_table *supp_page_table,
                                                void *uaddr, bool writable);
struct supp_page_entry *supp_page_set_file_data (struct supp_page_entry *entry,
                                                 struct file *file,
                                                 size_t page_read_bytes,
                                                 size_t page_zero_bytes);
void *supp_page_map_entry (uint32_t *pd, struct supp_page_entry *entry);
struct supp_page_entry *supp_page_lookup (struct supp_page_table *supp_page_table,
                                          void *uaddr);

#endif
