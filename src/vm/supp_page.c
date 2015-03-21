#include <vm/supp_page.h>

#include <debug.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <lib/kernel/list.h>
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
#include <vm/swap.h>

static struct supp_page_segment *segment_from_elem (const struct list_elem *e);
static bool supp_page_segment_contains (struct supp_page_segment *segment, void *uaddr);
static void setup_file_page (void *uaddr, void *kpage,
                             struct supp_page_segment *segment);
static void supp_page_install_page (void *uaddr, void *kpage,
                                    struct supp_page_segment *segment);

static struct supp_page_mapped *mapped_from_mapped_elem (const struct hash_elem *e);
static unsigned mapped_hash_func (const struct hash_elem *mapped_elem,
                                  void *aux UNUSED);
static bool mapped_less_func (const struct hash_elem *a,
                              const struct hash_elem *b,
                              void *aux UNUSED);
static void supp_page_free_mapped (struct hash_elem *mapped_elem,
                                   void *pagedir_);
static uint32_t get_page_read_bytes (void *segment_addr, void *uaddr,
                                     uint32_t segment_read_bytes);
static struct supp_page_mapped *lookup_mapped (struct supp_page_segment *segment,
                                               void *uaddr);


/* Initialize the given supplementary page table with the given page table. */
void
supp_page_table_init (struct supp_page_table *supp_page_table)
{
  list_init (&supp_page_table->segments);
}

/* Allocates a new supplementary page table segment, and insert it into the table. */
struct supp_page_segment *
supp_page_create_segment (struct supp_page_table *supp_page_table,
                          void *addr, bool writable, uint32_t size)
{
  struct supp_page_segment *segment = try_calloc (1, sizeof *segment);
  segment->addr = addr;
  segment->file_data = NULL;
  hash_init (&segment->mapped_pages, mapped_hash_func, mapped_less_func, NULL);
  segment->writable = writable;
  segment->size = size;

  list_push_back (&supp_page_table->segments, &segment->supp_elem);
  return segment;
}

/* Sets data for a segment that is read from a file. */
struct supp_page_segment *
supp_page_set_file_data (struct supp_page_segment *segment, struct file *file,
                         uint32_t offset, uint32_t read_bytes, bool is_mmapped)
{
  struct supp_page_file_data *file_data = try_calloc (1, sizeof *file_data);
  file_data->file = file;
  file_data->offset = offset;
  file_data->read_bytes = read_bytes;
  file_data->is_mmapped = is_mmapped;
  segment->file_data = file_data;
  return segment;
}

/* Looks up a user virtual address in the supplementary page table.
   Returns NULL if no segment can be found. */
struct supp_page_segment *
supp_page_lookup_segment (struct supp_page_table *supp_page_table, void *fault_addr)
{
  struct list_elem *current = list_begin (&supp_page_table->segments);
  while (current != list_end (&supp_page_table->segments))
    {
      struct supp_page_segment *segment = segment_from_elem (current);
      if (supp_page_segment_contains (segment, fault_addr))
        {
          return segment;
        }
      current = list_next (current);
    }
  return NULL;
}

/* Tries to get a frame and map the faulting page inside segment to this frame. */
void *
supp_page_map_addr (struct supp_page_table *supp_page_table, void *fault_addr)
{
  struct supp_page_segment *segment =
    supp_page_lookup_segment (supp_page_table, fault_addr);
  if (segment == NULL)
    {
      /* Attempt to access invalid page, so terminate the thread. */
      thread_exit ();
    }

  /* Calculate the address of the page that fault_addr is inside. */
  void *uaddr = pg_round_down (fault_addr);

  struct supp_page_mapped *mapped = try_calloc (1, sizeof *mapped);
  mapped->uaddr = uaddr;
  mapped->swap_slot_no = NOT_SWAP;
  hash_insert (&segment->mapped_pages, &mapped->mapped_elem);

  /* Try to get a frame from the frame table. */
  void *kpage = request_frame (PAL_NONE, mapped);
  if (kpage == NULL)
    {
      /* This should never happen. */
      PANIC ("Was not able to retrieve frame.");
    }

  /* if this is not NULL, it represents a page that needs to be read from a
     file. */
  struct supp_page_file_data *file_data = segment->file_data;
  if (file_data != NULL)
    {
      setup_file_page (uaddr, kpage, segment);
    }
  else
    {
      memset (kpage, 0, PGSIZE);
    }

  /* Map the user address to the frame. */
  supp_page_install_page (uaddr, kpage, segment);

  unpin_frame (kpage);

  return kpage;
}

/* Write a page back to the file it is from if it is mmapped.
   Returns false if it is not mmapped or the page is not a file page. */
bool
supp_page_write_mmapped (struct supp_page_mapped *mapped)
{
  struct supp_page_segment *segment = mapped->segment;
  if (segment->file_data != NULL)
    {
      struct supp_page_file_data *file_data = segment->file_data;
      if (file_data->is_mmapped &&
          (uint8_t *)mapped->uaddr < (uint8_t *)segment->addr + file_data->read_bytes)
        {
          uint32_t page_read_bytes =
            get_page_read_bytes (segment->addr, mapped->uaddr,
                                 segment->file_data->read_bytes);
          filesys_lock_acquire ();
          file_seek (file_data->file, (uint32_t)mapped->uaddr - (uint32_t)segment->addr);
          file_write (file_data->file, mapped->uaddr, page_read_bytes);
          filesys_lock_release ();
          return true;
        }
    }
  return false;
}

/* Frees all the memory used by a particular supplementary page table in a
   thread. */
void
supp_page_free_all (struct supp_page_table *supp_page_table,
                    uint32_t *pagedir)
{
  while (!list_empty (&supp_page_table->segments))
    {
      struct supp_page_segment *segment =
        segment_from_elem (list_begin (&supp_page_table->segments));
      supp_page_free_segment (segment, pagedir);
    }
}

void
supp_page_free_segment (struct supp_page_segment *segment,
                        uint32_t *pagedir)
{
  list_remove (&segment->supp_elem);
  segment->mapped_pages.aux = pagedir;
  hash_destroy (&segment->mapped_pages, supp_page_free_mapped);

  free (segment->file_data);
  free (segment);
}

/* Get the supp_page_segment wrapping a supp_elem. */
static struct supp_page_segment *
segment_from_elem (const struct list_elem *e)
{
  ASSERT (e != NULL);
  return list_entry (e, struct supp_page_segment, supp_elem);
}

/* Is the given uaddr within the given segment? */
static bool
supp_page_segment_contains (struct supp_page_segment *segment, void *uaddr)
{
  return segment->addr <= uaddr &&
    (uint8_t *)uaddr <= (uint8_t *)segment->addr + segment->size;
}

/* Reads file data into the kpage, for virtual user page at uaddr. */
static void
setup_file_page (void *uaddr, void *kpage, struct supp_page_segment *segment)
{
  struct supp_page_file_data *file_data = segment->file_data;
  uint32_t page_read_bytes = get_page_read_bytes (segment->addr, uaddr,
                                                  file_data->read_bytes);

  uint32_t offset_to_page = file_data->offset +
    ((uint32_t)uaddr - (uint32_t)segment->addr);

  /* Acquiring lock on the filesystem. We may be page faulting from code that
     has already acquired this lock, so we check first. */
  bool acquired_lock = false;
  if (!filesys_lock_held ())
    {
      acquired_lock = true;
      filesys_lock_acquire ();
    }

  /* Read the data into the page, error check, and then release the lock. */
  file_seek (file_data->file, offset_to_page);
  if (!read_page (kpage, file_data->file, page_read_bytes,
                  PGSIZE - page_read_bytes))
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

/* Installs a kpage with uaddr into the current thread's pagedir. */
static void
supp_page_install_page (void *uaddr, void *kpage,
                        struct supp_page_segment *segment)
{
  struct supp_page_mapped *mapped = try_calloc (1, sizeof *mapped);
  mapped->segment = segment;
  mapped->uaddr = uaddr;
  mapped->swap_slot_no = NOT_SWAP;
  /* TODO: Need to lock this. Otherwise eviction could mess things up */
  if (!install_page (uaddr, kpage, segment->writable))
    {
      free_frame (kpage);
      thread_exit ();
    }
}


/* mapped_pages hash functions */

/* Get the supp_page_mapped wrapping a mapped_elem. */
static struct supp_page_mapped *
mapped_from_mapped_elem (const struct hash_elem *e)
{
  ASSERT (e != NULL);
  return hash_entry (e, struct supp_page_mapped, mapped_elem);
}

static unsigned
mapped_hash_func (const struct hash_elem *mapped_elem, void *aux UNUSED)
{
  struct supp_page_mapped *mapped = mapped_from_mapped_elem (mapped_elem);
  /* sizeof returns how many bytes uaddr, the pointer itself (not what it is
     pointing to), takes up. */
  return hash_bytes (&mapped->uaddr, sizeof mapped->uaddr);
}

static bool
mapped_less_func (const struct hash_elem *a, const struct hash_elem *b,
                  void *aux UNUSED)
{
  struct supp_page_mapped *mapped_a = mapped_from_mapped_elem (a);
  struct supp_page_mapped *mapped_b = mapped_from_mapped_elem (b);
  return mapped_a->uaddr < mapped_b->uaddr;
}

/* Used as a hash_action_func for hash_destroy in supp_page_free_all.
   Frees the mapped page corresponding to a single hash_elem. */
static void
supp_page_free_mapped (struct hash_elem *mapped_elem, void *pagedir_)
{
  struct supp_page_mapped *mapped = mapped_from_mapped_elem (mapped_elem);
  supp_page_write_mmapped (mapped);
  if (mapped->swap_slot_no != NOT_SWAP)
    {
      swap_free_slot (mapped->swap_slot_no);
    }
  uint32_t *pagedir = (uint32_t *)pagedir_;
  free_frame (pagedir_get_page (pagedir, mapped->uaddr));
  pagedir_clear_page (pagedir, mapped->uaddr);
  free (mapped);
}

static uint32_t
get_page_read_bytes (void *segment_addr, void *uaddr, uint32_t segment_read_bytes)
{
  /* Calculate the bytes we have to read from the particular page at uaddr in
     the segment. */
  uint32_t page_read_bytes = 0;
  uint8_t *end_of_read_bytes = (uint8_t *)segment_addr + segment_read_bytes;
  /* We only have to read if the uaddr is within the part of the segment where we
     have to read from. */
  if ((uint8_t *)uaddr < end_of_read_bytes)
    {
      page_read_bytes = (uint32_t)end_of_read_bytes - (uint32_t)uaddr;
      page_read_bytes = page_read_bytes > PGSIZE ? PGSIZE : page_read_bytes;
    }

  return page_read_bytes;
}

/* Looks for a mapped page with the given uaddr in the given segment. */
static struct supp_page_mapped *
lookup_mapped (struct supp_page_segment *segment, void *uaddr)
{
  struct supp_page_mapped for_lookup;
  for_lookup.uaddr = uaddr;

  struct hash_elem *result = hash_find (&segment->mapped_pages, &for_lookup.mapped_elem);
  return result == NULL ? NULL : mapped_from_mapped_elem (result);
}
