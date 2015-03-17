#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <threads/palloc.h>

void frame_init (void);
void *request_frame (enum palloc_flags additional_flags, void *upage);
void free_frame (void *kpage);

#endif /* vm/frame.h */
