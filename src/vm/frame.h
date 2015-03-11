#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <lib/kernel/list.h>
#include <threads/synch.h>

/* The frame table allows requesting frames for mapping to virtual
   user addresses.
   Functions are provided for requesting an available frame.
   It also provides automatic eviction of an already allocated frame
   if no frames are available. */
struct frame_table
{
  struct lock table_lock; /* Synchronizes table between threads. */
  struct list allocated; /* Frames which have been allocated already. */
};

/* Meta-data about a frame. A frame is a physical storage unit in memory,
   page-aligned, and page-sized. Pages are stored in frames, though their
   virtual addresses may differ from the actual physical address. */
struct frame
{
  struct list_elem frame_elem; /* For placing frames inside frame_tables. */
  void *kpage; /* The kernel virtual address of the frame. */
};

void frame_init (void);
struct frame *request_frame (void);
void free_frame (struct frame *frame);

#endif
