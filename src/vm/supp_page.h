#ifndef VM_SUPP_PAGE_H
#define VM_SUPP_PAGE_H

#include <stdbool.h>
#include <stdint.h>
#include <lib/kernel/hash.h>

#include <filesys/file.h>
#include <filesys/off_t.h>

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
    struct list entries; /* Entries in this table. */
};

/* An entry in the supplementary page table.

   This is where the data for each segment in the supplementary page table is
   stored.

   Segments may have originated from a file (they are read from a file into
   memory). If they did not, then the file_data field will be set to NULL. */
struct supp_page_entry
  {
    struct list_elem supp_elem; /* For placing this into a supp_page_table. */
    void *uaddr; /* The virtual user address this segment begins at. */

    struct supp_page_file_data *file_data; /* File data */

    /* Other properties */
    bool writable; /* Whether this segment is writable or not. */
    uint32_t size; /* The size of the segment. */
  };

/* Data pertaining to a page that is read from a file. */
struct supp_page_file_data
  {
    struct file *file; /* The file that this page is read from. NULL if not a file. */
    uint32_t offset; /* Offset from which segment starts from, in file. */
    /* We will read read_bytes number of bytes from file and zero the remaining
       bytes in the segment. */
    uint32_t read_bytes;
  };

void supp_page_table_init (struct supp_page_table *supp_page_table);
struct supp_page_entry *supp_page_create_entry (struct supp_page_table *supp_page_table,
                                                void *uaddr, bool writable);
struct supp_page_entry *supp_page_set_file_data (struct supp_page_entry *entry,
                                                 struct file *file,
                                                 off_t offset,
                                                 size_t read_bytes);
struct supp_page_entry *supp_page_lookup (struct supp_page_table *supp_page_table,
                                          void *uaddr);
struct supp_page_entry **supp_page_lookup_range (struct supp_page_table *supp_page_table,
                                                 void *base_addr,
                                                 struct supp_page_entry **buffer,
                                                 unsigned number);
void *supp_page_map_entry (struct supp_page_entry *entry, void *uaddr);
void supp_page_map_entries (struct supp_page_entry **entry_array,
                            unsigned num_of_entries);
void supp_page_free_all (struct supp_page_table *supp_page_table,
                         uint32_t *pagedir);

#endif  /* vm/supp_page.h */
