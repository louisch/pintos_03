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
  hash_init (&supp_page_table->table, supp_page_hash_func,
             supp_page_less_func, NULL);
}

/* Allocates a new supplementary page table entry, and insert it into the hash. */
struct supp_page_entry *
supp_page_create_entry (struct supp_page_table *supp_page_table,
                        void *uaddr, bool writable)
{
  struct supp_page_entry *entry = calloc (1, sizeof *entry);
  entry->uaddr = uaddr;
  entry->file = NULL;
  entry->offset = 0;
  entry->writable = writable;

  hash_insert (&supp_page_table->table, &entry->supp_elem);
  return entry;
}

/* Sets data for a page that is read from a file. */
struct supp_page_entry *
supp_page_set_file_data (struct supp_page_entry *entry, struct file *file,
                         off_t offset, size_t page_read_bytes, size_t page_zero_bytes)
{
  entry->file = file;
  entry->offset = offset;
  entry->page_read_bytes = page_read_bytes;
  entry->page_zero_bytes = page_zero_bytes;
  return entry;
}

/* Try to get a frame and map entry->upage to this frame in the page table. */
void *
supp_page_map_entry (struct supp_page_entry *entry)
{
  ASSERT (entry != NULL);
  ASSERT (entry->file != NULL);
  ASSERT (entry->page_read_bytes + entry->page_zero_bytes == PGSIZE);

  void *kpage = request_frame (PAL_NONE);
  if (kpage == NULL)
    {
      /* This should never happen. */
      PANIC ("Was not able to retrieve frame.");
    }
  file_seek (entry->file, entry->offset);
  if (!read_page (kpage, entry->file, entry->page_read_bytes,
                  entry->page_zero_bytes))
    {
      free_frame (kpage);
      thread_exit ();
    }

  if (!install_page (entry->uaddr, kpage, entry->writable))
    {
      free_frame (kpage);
      thread_exit ();
    }
  return kpage;
}

/* Looks up a user virtual address in the supplementary page table.
   Returns NULL if no entry can be found. */
struct supp_page_entry *
supp_page_lookup (struct supp_page_table *supp_page_table, void *uaddr)
{
  struct supp_page_entry for_hashing;
  for_hashing.uaddr = pg_round_down (uaddr);
  struct hash_elem *found = hash_find (&supp_page_table->table,
                                       &for_hashing.supp_elem);
  return found == NULL ? NULL : supp_page_from_elem (found);
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
