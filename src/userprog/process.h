#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include <kernel/list.h>
#include <kernel/hash.h>
#include <user/syscall.h>
#include "threads/thread.h"
#include "filesys/file.h"
#include "threads/synch.h"

/* Data for a process used for syscalls. */
typedef struct process_info
{
  pid_t pid;
  /* The thread owned by the process */
  tid_t tid;

  struct lock children_lock;
  /* Tracks the children of the process */
  struct list children;
  /* Allows this to be placed in another process' children list */
  struct list_elem child_elem;

  /* For placing process_info in hash table mapping pids to process_info. */
  struct hash_elem process_elem;

  unsigned fd_counter;
  struct hash open_files;

  int exit_status;
} process_info;

int process_add_file (struct file *);
struct file* process_fetch_file (int fd);
struct file* process_remove_file (int fd);

void process_info_init (void);
tid_t process_execute (const char *file_name);
pid_t process_execute_pid (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

process_info *process_current (void);
process_info *process_get_info (pid_t pid);

#endif /* userprog/process.h */
