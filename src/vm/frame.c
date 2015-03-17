#include <vm/frame.h>

#include <debug.h>
#include <stdint.h>
#include <stdio.h>

#include <lib/kernel/hash.h>
#include <lib/kernel/list.h>
#include <threads/malloc.h>
#include <threads/palloc.h>
#include <threads/synch.h>
#include <threads/thread.h>

#include "userprog/pagedir.h"

/* The frame table allows requesting frames for mapping to virtual
   user addresses.
   Functions are provided for requesting an available frame.
   It also provides automatic eviction of an already allocated frame
   if no frames are available. */
struct frame_table
{
  struct lock table_lock; /* Synchronizes table between threads. */
  struct hash allocated;  /* Frames which have been allocated already. */
  struct list eviction_queue; /* List facilitating eviction. */
};

/* Meta-data about a frame. A frame is a physical storage unit in memory,
   page-aligned, and page-sized. Pages are stored in frames, though their
   virtual addresses may differ from the actual physical address. */
struct frame
{
  struct hash_elem frame_elem; /* For placing frames inside frame_tables. */
  struct list_elem eviction_elem;
  uint32_t *pd; /* The owner thread's page directory. */
  void *upage;  /* The user virtual address of the frame. */
  void *kpage;  /* The kernel virtual address of the frame. */
};

static struct frame *allocated_find_frame (void *kpage);
static unsigned allocated_hash_func (const struct hash_elem *e, void *aux UNUSED);
static bool allocated_less_func (const struct hash_elem *a, const struct hash_elem *b,
                                 void *aux UNUSED);
static struct frame *frame_from_hash_elem (const struct hash_elem *e);
static void *evict_frame (void);
static void free_frame_stat (struct frame *frame);
static struct frame *frame_from_eviction_elem (const struct list_elem *e);

static struct frame_table frames;

/* Initializes the frame table. */
void
frame_init (void)
{
  lock_init (&frames.table_lock);
  lock_acquire (&frames.table_lock);
  hash_init (&frames.allocated, allocated_hash_func, allocated_less_func, NULL);
  list_init (&frames.eviction_queue);
  lock_release (&frames.table_lock);
}

/* Returns the address of an available frame from the user pool.
   Returns the kernel virtual address of the frame. A user virtual address can
   be mapped to this address in the page table. */
void *
request_frame (enum palloc_flags additional_flags, void *upage)
{
  lock_acquire (&frames.table_lock);
  /* For now, evict pages out of the system. */
  void *page = palloc_get_page (additional_flags | PAL_USER);
  if (page == NULL)
    {
      printf ("#Evicting frames out of a window!\n░░░░░░░░▄▀▀████████▀██████▄▄▄▄▄▄░░░░░░\n░░░░░░▄▀░▄▀░░▄▄▄▄▄░▀▄▄▄▄▄▄▀▀▀▄▄░▀▄░░░░\n░░░░▄▀░░▀░▄▄▀░░░░░▀█▄░░░▄▀▀▀▀▄▀▀░░█░░░\n░░░▄▀░░░░▄▀░▄███▀▀▄░▀░░█░▄▄▄▄▄░░▄▄█▄░░\n░▄▀░░▄▀▀▄░░▀▀░▄▀▀▀██░░░██▀▀▀░▀▀░▀▀▀██▄\n█░░▄▀░▄▄░▀▀░▀▀░░▄░▄▄░░░░█▄░░▄▄▀▀▀█▄░██\n█░░█░▀█░▀█▄▄▄▄▄▀░█░▄▄▄░░░▀██▄░░░▄░░▄██\n█░░░▀░░█▄▄█░░▀▀▀█▄▄░░▀░▀▀▀▀░░▄▄▀▀█░█▀░\n░▀▄░░░░░▀▄▀█▀█▄▄█▄░▀▀█▀▀█▀▀▀█░▄████▀░░\n░░▀▄░░░░░▄▀█▄░▀████████████████████░░░\n░░░░▀▄▄░░░▀▄▀▀▄█░░░░░█░░█░░█░█░▄▀░█▄░░\n░░░░░░░▀▄░░░▀▄▄░▀▀▄▄█▀▀▀▀▀▀▀▀▀▀▄░░░█▄░\n░░░░░░░░░▀▀▄▄░░▀▀▄▄░░░▄▄▄▄▄▄▄░▀▄▄░░░█░\n░░░░░░░░░░░░░▀▀▀▄▄▄▀▀▀▀▀▀▀▀▀▀▀▀▀░░▄▀░░\n░░░░░░░░░░░░░░░░░░░▀▀▄▄▄▄▄▄▄▄▄▄▄▄▀░░░░\n");
      page = evict_frame ();
    }

  struct frame *frame = calloc (1, sizeof *frame);
  frame->kpage = page;
  frame->upage = upage;
  frame->pd = thread_current ()->pagedir;
  hash_insert (&frames.allocated, &frame->frame_elem);
  list_push_back (&frames.eviction_queue, &frame->eviction_elem);
  lock_release (&frames.table_lock);

  return frame->kpage;
}

/* #TROLL HAXX */
/* Selects a frame from frames and evicts it to oblivion
   using the second chance algorithm. */
static void *
evict_frame (void)
{
  ASSERT (list_empty (&frames.eviction_queue));

  struct list_elem *e = list_front(&frames.eviction_queue);
  struct frame *f = frame_from_eviction_elem (e);
  while (e != list_end (&frames.eviction_queue)
         && !pagedir_is_accessed (f->pd, f->upage))
  {
    pagedir_set_accessed (f->pd, f->upage, false);
    e = list_next (e);
    f = frame_from_eviction_elem (e);
    list_push_back (&frames.eviction_queue,
                     list_pop_front (&frames.eviction_queue));
  }

  pagedir_clear_page (f->pd, f->upage);
  void *page = f->kpage;
  // SWAPP HAXX
  free_frame_stat (f);

  return page;
}

/* Frees the page held inside the frame, and the given struct frame itself. */
void
free_frame (void *kpage)
{
  if (kpage == NULL)
    {
      return;
    }
  lock_acquire (&frames.table_lock);
  struct frame *frame = allocated_find_frame (kpage);
  free_frame_stat (frame);
  lock_release (&frames.table_lock);
}

static void
free_frame_stat (struct frame *frame)
{
  if (frame != NULL)
    {
      hash_delete (&frames.allocated, &frame->frame_elem);
      list_remove (&frame->eviction_elem);
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

  return frame_from_hash_elem (found_elem);
}

/* allocated hash function */
static unsigned
allocated_hash_func (const struct hash_elem *e, void *aux UNUSED)
{
  void *kpage = frame_from_hash_elem (e)->kpage;
  /* sizeof returns how many bytes kpage, the pointer itself (not what it is
     pointing to), takes up. */
  return hash_bytes (&kpage, sizeof kpage);
}

/* allocated less than function */
static bool
allocated_less_func (const struct hash_elem *a, const struct hash_elem *b,
                     void *aux UNUSED)
{
  void *kpage_a = frame_from_hash_elem (a)->kpage;
  void *kpage_b = frame_from_hash_elem (b)->kpage;
  return kpage_a < kpage_b;
}

/* Wrapper function for the hash_entry macro, specific to frame_table.allocated */
static struct frame *
frame_from_hash_elem (const struct hash_elem *e)
{
  ASSERT (e != NULL);
  return hash_entry (e, struct frame, frame_elem);
}

/* Wrapper for the list_entry macro, specific to frame_table.eviction_queue. */
static struct frame *
frame_from_eviction_elem (const struct list_elem *e)
{
  ASSERT (e != NULL);
  return list_entry (e, struct frame, eviction_elem);
}
