#include <vm/frame.h>

#include <debug.h>
#include <stdint.h>

#include <lib/kernel/hash.h>
#include <threads/malloc.h>
#include <threads/palloc.h>
#include <threads/synch.h>

/* Meta-data about a frame. A frame is a physical storage unit in memory,
   page-aligned, and page-sized. Pages are stored in frames, though their
   virtual addresses may differ from the actual physical address. */
struct frame
{
  struct hash_elem frame_elem; /* For placing frames inside frame_tables. */
  void *kpage; /* The kernel virtual address of the frame. */
};

static struct frame *allocated_find_frame (void *kpage);
static unsigned allocated_hash_func (const struct hash_elem *e, void *aux UNUSED);
static bool allocated_less_func (const struct hash_elem *a, const struct hash_elem *b,
                                 void *aux UNUSED);
static struct frame *frame_from_elem (const struct hash_elem *e);

static struct frame_table frames;

/* Initializes the frame table. */
void
frame_init (void)
{
  lock_init (&frames.table_lock);
  lock_acquire (&frames.table_lock);
  hash_init (&frames.allocated, allocated_hash_func, allocated_less_func, NULL);
  lock_release (&frames.table_lock);
}

/* Returns the address of an available frame from the user pool.
   Returns the kernel virtual address of the frame. A user virtual address can
   be mapped to this address in the page table. */
void *
request_frame (enum palloc_flags additional_flags)
{
  /* For now, just panic (with PAL_ASSERT) if no page can
     be fetched. TODO: Implement eviction so this doesn't
     need to happen. */
  void *page = palloc_get_page (additional_flags | PAL_USER | PAL_ASSERT);
  if (page == NULL)
    {
      PANIC ("Could not get page");
    }

  struct frame *frame = calloc (1, sizeof *frame);
  frame->kpage = page;
  lock_acquire (&frames.table_lock);
  hash_insert (&frames.allocated, &frame->frame_elem);
  lock_release (&frames.table_lock);

  return frame->kpage;
}

/* Frees the page held inside the frame, and the given struct frame itself. */
void
free_frame (void *kpage)
{
  if (kpage == NULL)
    {
      return;
    }

  struct frame *frame = allocated_find_frame (kpage);
  if (frame != NULL)
    {
      hash_delete (&frames.allocated, &frame->frame_elem);
      palloc_free_page (frame->kpage);
      free (frame);
    }
}

/* Finds the frame from the allocated hash table, given the kernel virtual
   address of the frame.
   Returns NULL if not found. */
static struct frame *
allocated_find_frame (void *kpage)
{
  struct frame for_hashing;
  for_hashing.kpage = kpage;

  struct hash_elem *found_elem = hash_find (&frames.allocated,
                                            &for_hashing.frame_elem);
  if (found_elem == NULL)
    {
      return NULL;
    }

  return frame_from_elem (found_elem);
}

/* allocated hash function */
static unsigned
allocated_hash_func (const struct hash_elem *e, void *aux UNUSED)
{
  void *kpage = frame_from_elem (e)->kpage;
  /* sizeof returns how many bytes kpage, the pointer itself (not what it is
     pointing to), takes up. */
  return hash_bytes (&kpage, sizeof kpage);
}

/* allocated less than function */
static bool
allocated_less_func (const struct hash_elem *a, const struct hash_elem *b,
                     void *aux UNUSED)
{
  void *kpage_a = frame_from_elem (a)->kpage;
  void *kpage_b = frame_from_elem (b)->kpage;
  return kpage_a < kpage_b;
}

/* Wrapper function for the hash_entry macro, specific to frame_table.allocated */
static struct frame *
frame_from_elem (const struct hash_elem *e)
{
  ASSERT (e != NULL);
  return hash_entry (e, struct frame, frame_elem);
}
