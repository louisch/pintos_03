#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include <kernel/list.h>
#include <kernel/hash.h>
#include <user/syscall.h>
#include "threads/thread.h"

/* Data for a process used for syscalls. */
typedef struct
{
  pid_t pid;
  /* The thread owned by the process */
  tid_t tid;

  struct lock children_lock;
  /* Tracks the children of the process */
  struct list children;
  /* Allows this to be placed in another process' children list */
  struct list_elem child_elem;

  /* Allows the process_info to be placed in the static hash table
     processes. */
  struct hash_elem process_elem;
} process_info;

void process_info_init (void);
tid_t process_execute (const char *file_name);
pid_t process_execute_pid (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

process_info *process_get_info (pid_t pid);

#endif /* userprog/process.h */
