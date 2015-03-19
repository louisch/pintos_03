#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <stdint.h>

#define NOT_SWAP INT32_MAX

/* Type for swap slot numbers. */
typedef uint32_t slot_no;

void swap_init (void);
slot_no swap_write (void *);
void swap_retrieve (slot_no, void *);
void swap_free_slot (slot_no slot);
// void free_used_frames (void); // ARG HAXX

#endif /* vm/swap.h */
