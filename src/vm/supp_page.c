#include <vm/supp_page.h>

#include <debug.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <lib/kernel/hash.h>

#include <filesys/file.h>
#include <filesys/filesys_lock.h>
#include <filesys/off_t.h>
#include <threads/malloc.h>
#include <threads/palloc.h>
#include <threads/thread.h>
#include <threads/vaddr.h>
#include <userprog/pagedir.h>
#include <userprog/read_page.h>
#include <userprog/install_page.h>
#include <vm/frame.h>

static struct supp_page_entry *supp_page_from_elem (const struct hash_elem *e);
static bool supp_page_entry_contains (struct supp_page_entry *entry, void *uaddr);
static void supp_page_free (struct hash_elem *entry_elem, void *aux UNUSED);

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
                        void *uaddr, bool writable, uint32_t size)
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
  entry->size = size;

  hash_insert (&supp_page_table->hash, &entry->supp_elem);
  return entry;
}

/* Sets data for a page that is read from a file. */
struct supp_page_entry *
supp_page_set_file_data (struct supp_page_entry *entry, struct file *file,
                         uint32_t offset, uint32_t read_bytes)
{
  struct supp_page_file_data *file_data = calloc (1, sizeof *file_data);
  file_data->file = file;
  file_data->offset = offset;
  file_data->read_bytes = read_bytes;
  entry->file_data = file_data;
  return entry;
}

/* Looks up a user virtual address in the supplementary page table.
   Returns NULL if no entry can be found. */
struct supp_page_entry *
supp_page_lookup (struct supp_page_table *supp_page_table, void *uaddr)
{
  struct supp_page_entry *found_entry = NULL;
  struct list_elem *current = list_begin (&supp_page_table->entries);
  while (current != list_end (&supp_page_table->entries))
    {
      struct supp_page_entry *entry = supp_page_from_elem (current);
      if (supp_page_entry_contains (entry, uaddr))
        {
          found_entry = entry;
          break;
        }
      current = list_next (current);
    }
  return found_entry;
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

/* Tries to get a frame and map the faulting page inside entry to this frame. */
void *
supp_page_map_entry (struct supp_page_entry *entry, void *uaddr)
{
  ASSERT (entry != NULL);

  /* Try to get a frame and read the right data into it. */
  void *kpage = NULL;
  /* Calculate the offset from entry->uaddr to the page that uaddr is in. */
  uint32_t offset_to_page = ((uint32_t)pg_round_down (uaddr) - (uint32_t)entry->uaddr);
  void *uaddr = (uint8_t *)entry->uaddr + offset_to_page;

  /* if this is not NULL, it represents a page that needs to be read from a
     file. */
  struct supp_page_file_data *file_data = entry->file_data;
  if (file_data != NULL)
    {
      /* Try to get a frame from the frame table. */
      kpage = request_frame (PAL_NONE, uaddr);

      /* Calculate how many bytes of the file we are reading, and the offset into
         the file to seek to. */
      uint32_t page_read_size = offset_to_page < file_data->read_bytes ?
        file_data->read_bytes - offset_to_page : 0;
      page_read_size = page_read_size > PGSIZE ? PGSIZE : page_read_size;

      bool acquired_lock = false;
      if (!filesys_lock_held ())
        {
          acquired_lock = true;
          filesys_lock_acquire ();
        }

      file_seek (file_data->file, file_data->offset + offset_to_page);
      if (!read_page (kpage, file_data->file, page_read_size,
                      PGSIZE - page_read_size))
        {
          if (acquired_lock)
            {
              filesys_lock_release ();
            }
          free_frame (kpage);
          thread_exit ();
        }
      if (acquired_lock)
        {
          filesys_lock_release ();
        }
    }
  else
    {
      kpage = request_frame (PAL_ZERO, );
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

/* Frees all the memory used by a particular supplementary page table in a
   thread. */
void
supp_page_free_all (struct supp_page_table *supp_page_table,
                    uint32_t *pagedir)
{
  supp_page_table->hash.aux = pagedir;
  hash_destroy (&supp_page_table->hash, supp_page_free);
}

/* Get the supp_page_entry wrapping a supp_elem. */
static struct supp_page_entry *
supp_page_from_elem (const struct list_elem *e)
{
  ASSERT (e != NULL);
  return list_entry (e, struct supp_page_entry, supp_elem);
}

static bool
supp_page_entry_contains (struct supp_page_entry *entry, void *uaddr)
{
  return entry->uaddr <= uaddr &&
    (uint8_t *)uaddr <= (uint8_t *)entry->uaddr + size;
}

/* Used as a hash_action_func for hash_destroy in supp_page_free_all.
   Frees the page corresponding to a single hash_elem. */
static void
supp_page_free (struct hash_elem *entry_elem, void *pagedir_)
{
  struct supp_page_entry *entry = supp_page_from_elem (entry_elem);
  uint32_t *pagedir = (uint32_t *)pagedir_;
  free_frame (pagedir_get_page (pagedir, entry->uaddr));
  pagedir_clear_page (pagedir, entry->uaddr);
  free (entry->file_data);
  free (entry);
}
