#ifndef VM_STACK_GROWTH_H
#define VM_STACK_GROWTH_H

#include <stdbool.h>

#define STACK_SIZE 8388608 //8mib in bytes

bool stack_should_grow (void* esp, void* fault_addr);
void grow_stack (void *esp);

#endif