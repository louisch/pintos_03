#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

/* TODO: remove this dirty hack (it's a duplicate typedef of the one in
   lib/user/syscall.h */
typedef int pid_t;

void syscall_init (void);

#endif /* userprog/syscall.h */
