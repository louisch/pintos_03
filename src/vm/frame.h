#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <threads/palloc.h>
#include "vm/supp_page.h"

void frame_init (void);
void pin_frame (void *kpage);
void unpin_frame (void *kpage);
void *request_frame (enum palloc_flags additional_flags,
	                 struct supp_page_mapped *mapped);
void free_frame (void *kpage);

#endif /* vm/frame.h */
