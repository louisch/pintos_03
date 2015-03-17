#include <vm/supp_page.h>

#include <debug.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <lib/kernel/hash.h>

#include <filesys/file.h>
#include <filesys/off_t.h>
#include <threads/malloc.h>
#include <threads/palloc.h>
#include <threads/thread.h>
#include <threads/vaddr.h>
#include <userprog/pagedir.h>
#include <userprog/read_page.h>
#include <userprog/install_page.h>
#include <vm/frame.h>

static unsigned supp_page_hash_func (const struct hash_elem *e, void *aux UNUSED);
static bool supp_page_less_func (const struct hash_elem *a,
                                 const struct hash_elem *b,
                                 void *aux UNUSED);
static struct supp_page_entry *supp_page_from_elem (const struct hash_elem *e);

/* Initialize the given supplementary page table with the given page table. */
void
supp_page_table_init (struct supp_page_table *supp_page_table)
{
  hash_init (&supp_page_table->hash, supp_page_hash_func,
             supp_page_less_func, NULL);
}

/* Allocates a new supplementary page table entry, and insert it into the hash. */
struct supp_page_entry *
supp_page_create_entry (struct supp_page_table *supp_page_table,
                        void *uaddr, bool writable)
{
  struct supp_page_entry *entry = calloc (1, sizeof *entry);
  if (entry == NULL)
    {
      printf ("Could not allocate memory for supplementary page table entry.\n");
      thread_exit ();
    }
  entry->uaddr = uaddr;
  entry->file_data = NULL;
  entry->writable = writable;

  hash_insert (&supp_page_table->hash, &entry->supp_elem);
  return entry;
}

/* Sets data for a page that is read from a file. */
struct supp_page_entry *
supp_page_set_file_data (struct supp_page_entry *entry, struct file *file,
                         off_t offset, size_t page_read_bytes, size_t page_zero_bytes)
{
  struct supp_page_file_data *file_data = calloc (1, sizeof *file_data);
  file_data->file = file;
  file_data->offset = offset;
  file_data->page_read_bytes = page_read_bytes;
  file_data->page_zero_bytes = page_zero_bytes;
  entry->file_data = file_data;
  return entry;
}

/* Try to get a frame and map entry->upage to this frame in the page table.
   Non-file pages will simply be zeroed out. File pages will have their data
   read from the file that entry->file_data->file points to. */
void *
supp_page_map_entry (struct supp_page_entry *entry)
{
  ASSERT (entry != NULL);

  /* Try to get a frame and read the right data into it. */
  void *kpage = NULL;
  /* if this is not NULL, it represents a page that needs to be read from a
     file. */
  struct supp_page_file_data *file_data = entry->file_data;
  if (file_data != NULL)
    {
      ASSERT (file_data->page_read_bytes + file_data->page_zero_bytes == PGSIZE);
      /* Try to get a frame from the frame table. */
      kpage = request_frame (PAL_NONE);
      file_seek (file_data->file, file_data->offset);
      if (!read_page (kpage, file_data->file, file_data->page_read_bytes,
                      file_data->page_zero_bytes))
        {
          // TODO: fix (free all the thread's frames, somehow)
          free_frame (kpage);
          thread_exit ();
        }
    }
  else
    {
      kpage = request_frame (PAL_ZERO);
    }

  if (kpage == NULL)
    {
      /* This should never happen. */
      PANIC ("Was not able to retrieve frame.");
    }

  /* Map the user address to the frame. */
  if (!install_page (entry->uaddr, kpage, entry->writable))
    {
      free_frame (kpage);
      thread_exit ();
    }
  return kpage;
}
 
/* Like supp_page_map, but works on an array of entries. */
void
supp_page_map_entries (struct supp_page_entry **entry_array, unsigned num_of_entries)
{
  unsigned index;
  for (index = 0; index < num_of_entries; index++)
    {
      supp_page_map_entry (entry_array[index]);
    }
}

/* Looks up a user virtual address in the supplementary page table.
   Returns NULL if no entry can be found. */
struct supp_page_entry *
supp_page_lookup (struct supp_page_table *supp_page_table, void *uaddr)
{
  struct supp_page_entry for_hashing;
  for_hashing.uaddr = pg_round_down (uaddr);
  struct hash_elem *found = hash_find (&supp_page_table->hash,
                                       &for_hashing.supp_elem);
  return found == NULL ? NULL : supp_page_from_elem (found);
}

/* Like supp_page_lookup, but works on a range of addresses. */
struct supp_page_entry **
supp_page_lookup_range (struct supp_page_table *supp_page_table, void *base_addr,
                        struct supp_page_entry **buffer, unsigned number)
{
  ASSERT (buffer != NULL);

  unsigned index = 0;
  while (index != number)
    {
      buffer[index] = supp_page_lookup (supp_page_table,
                                        (uint8_t *)base_addr + index * PGSIZE);
      index++;
    }
  return buffer;
}

/* Used for setting up the hash table. Gets hash value for hash elements. */
static unsigned
supp_page_hash_func (const struct hash_elem *e, void *aux UNUSED)
{
  void *uaddr = supp_page_from_elem (e)->uaddr;
  /* sizeof returns how many bytes uaddr, the pointer itself (not what it is
     pointing to), takes up. */
  return hash_bytes (&uaddr, sizeof uaddr);
}

/* Used for setting up hash table. Orders hash elements. */
static bool
supp_page_less_func (const struct hash_elem *a, const struct hash_elem *b,
                     void *aux UNUSED)
{
  void *upage_a = supp_page_from_elem (a)->uaddr;
  void *upage_b = supp_page_from_elem (b)->uaddr;
  return upage_a < upage_b;
}

/* Get the supp_page_entry wrapping a supp_elem. */
static struct supp_page_entry *
supp_page_from_elem (const struct hash_elem *e)
{
  ASSERT (e != NULL);
  return hash_entry (e, struct supp_page_entry, supp_elem);
}
