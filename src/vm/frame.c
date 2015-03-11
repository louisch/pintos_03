#include <lib/kernel/list.h>
#include <threads/malloc.h>
#include <threads/palloc.h>
#include <threads/synch.h>

#include <vm/frame.h>

static struct frame_table frames;

/* Initializes the frame table. */
void
frame_init (void)
{
  lock_init (&frames.table_lock);
  lock_acquire (&frames.table_lock);
  list_init (&frames.allocated);
  lock_release (&frames.table_lock);
}

/* Returns an available frame from the user pool. A user virtual address can be
   mapped to the kernel virtual address of this frame. */
struct frame *
request_frame (void)
{
  /* For now, just panic (with PAL_ASSERT) if no page can
     be fetched. TODO: Implement eviction so this doesn't
     need to happen. */
  void *page = palloc_get_page (PAL_USER | PAL_ASSERT);

  struct frame *frame = calloc (1, sizeof *frame);
  frame->kpage = page;
  lock_acquire (&frames.table_lock);
  list_push_back (&frames.allocated, &frame->frame_elem);
  lock_release (&frames.table_lock);

  return frame;
}

/* Frees the page held inside the frame, and the given struct frame itself. */
void
free_frame (struct frame *frame)
{
  list_remove (&frame->frame_elem);
  palloc_free_page (frame->kpage);
  free (frame);
}
