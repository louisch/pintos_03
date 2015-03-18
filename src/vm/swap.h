#ifndef VM_SWAP_H
#define VM_SWAP_H

/* Type for swap slot numbers. */
typedef int slot_no;

void swap_init (void);
void swap_insert (void *);
void free_used_frames (void); // ARG HAXX

#endif /* vm/swap.h */
