#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <lib/kernel/hash.h>
#include <threads/palloc.h>
#include <threads/synch.h>

/* The frame table allows requesting frames for mapping to virtual
   user addresses.
   Functions are provided for requesting an available frame.
   It also provides automatic eviction of an already allocated frame
   if no frames are available. */
struct frame_table
  {
    struct lock table_lock; /* Synchronizes table between threads. */
    struct hash allocated; /* Frames which have been allocated already. */
  };

void frame_init (void);
void *request_frame (enum palloc_flags additional_flags);
void free_frame (void *kpage);

#endif /* vm/frame.h */
