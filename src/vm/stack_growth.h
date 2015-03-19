#ifndef VM_STACK_GROWTH_H
#define VM_STACK_GROWTH_H

#include <stdbool.h>
#include <stdint.h>

#define STACK_SIZE 8388608 /* 8mib in bytes */
/* The maximum number of bytes that fault_addr and esp can differ by, before a
   stack growth attempt is considered invalid. */
#define MAX_ESP_DIVERGENCE 64

void stack_growth_init (void);
bool is_stack_access (void *fault_addr);
bool is_valid_stack_access (void *fault_addr, void *esp);
uint8_t *maximum_stack_addr (void);

#endif /* vm/stack_growth.h */
