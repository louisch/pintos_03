#ifndef VM_SUPP_PAGE_H
#define VM_SUPP_PAGE_H

#include <stdbool.h>
#include <stdint.h>
#include <lib/kernel/list.h>
#include <lib/kernel/hash.h>

#include <filesys/file.h>
#include <filesys/off_t.h>
#include <vm/swap.h>

/* The supplementary page table keeps track of additional information on each
   page that the page table (in pagedir.h) cannot.

   This is used during page faults, to find out where the data for a virtual
   page is, so that the data can be read into memory, and then mapped in the
   page table. Pages may have been swapped out into swap space, or they may
   simply be in a file, or in the case of a stack growth, the page may not exist
   anywhere yet, and it simply needs to be created.

   It is organised in terms of segments. For the supplementary page table,
   segments are viewed as consecutive sequences of virtual pages. For example,
   the code segment of an executable is mapped as a consecutive sequence of
   virtual pages in virtual memory. */
struct supp_page_table
  {
    struct list segments; /* Entries in this table. */
  };

/* This is where the data for each segment that the user wants to load into
   memory is stored. Information for reading pages from this segment into
   memory is kept.

   Segments may have originated from a file (they are read from a file into
   memory). If they did not, then the file_data field will be set to NULL.

   Pages that lie in file segments are read from the file itself, using the
   file_data, while pages from non-file segments are simply zeroed out on
   creation.

   Pages which have been mapped already keep track of whether they are in swap
   or not. */
struct supp_page_segment
  {
    struct list_elem supp_elem; /* For placing this into a supp_page_table. */
    void *addr; /* The virtual user address this segment begins at. */

    struct supp_page_file_data *file_data; /* File data */
    struct hash mapped_pages; /* Previously mapped pages. */

    /* Other properties */
    bool writable; /* Whether this segment is writable or not. */
    uint32_t size; /* The size of the segment. */
  };

/* Data pertaining to a segment that has data which exists in a file. */
struct supp_page_file_data
  {
    struct file *file; /* The file that this page is read from. NULL if not a file. */
    uint32_t offset; /* Offset from which segment starts from, in file. */
    /* This is the number of bytes to read from the file, starting from the
       offset. If the size of the segment is larger than read_bytes, then the
       remaining bytes are zeroed out. (Of course, this is all read lazily, page
       by page, in whatever order the user decides to access the data). */
    uint32_t read_bytes;
    bool is_mmapped;
  };

/* Represents a page that has been mapped already in the pagedir. */
struct supp_page_mapped
  {
    struct hash_elem mapped_elem; /* For placing this in supp_page_segment. */
    struct supp_page_segment *segment; /* A pointer back to the segment that contains this. */
    void *uaddr; /* The virtual user address this page begins at. */
    slot_no swap_slot_no; /* Slot number of this page in swap, if it lies in swap. */
  };

void supp_page_table_init (struct supp_page_table *supp_page_table);
struct supp_page_segment *supp_page_create_segment (struct supp_page_table *supp_page_table,
                                                    void *addr, bool writable,
                                                    uint32_t size);
struct supp_page_segment *supp_page_set_file_data (struct supp_page_segment *segment,
                                                   struct file *file,
                                                   uint32_t offset,
                                                   uint32_t read_bytes,
                                                   bool is_mmapped);
struct supp_page_segment *supp_page_lookup_segment (struct supp_page_table *supp_page_table,
                                                    void *uaddr);
void *supp_page_map_addr (struct supp_page_table *supp_page_table, void *fault_addr);
void supp_page_free_all (struct supp_page_table *supp_page_table,
                         uint32_t *pagedir);
void supp_page_free_segment (struct supp_page_table *supp_page_table,
                             struct supp_page_segment *segment,
                             uint32_t *pagedir);

#endif  /* vm/supp_page.h */
