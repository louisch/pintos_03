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
  /* Process ID. */
  pid_t pid;
  /* ID of the thread owned by the process. */
  tid_t tid;

  /* Exit status of thread. */
  int exit_status;
  /* Lock to synchronise access to children hashtable. */
  struct lock children_lock;
  /* Tracks the process's children, running or terminated,
     which have not been waited on. */
  struct hash children;
  /* Pointer to parent's child_info struct. */
  struct child_info *parent_child_info;

  /* For placing process_info in hash table mapping pids to process_info. */
  struct hash_elem process_elem;

  /* TODO: Add comments */
  unsigned fd_counter;
  struct hash open_files;

} process_info;

typedef struct child_info
{
  /* Child thread's ID, used when checking if a child belongs to a parent. */
  tid_t tid;
  /* Exit status of thread. */
  int exit_status;
  /* Indicates whether the child process is still running. */
  bool running;
  /* Pointer to parent wait semaphore. Is NULL if the parent is not waiting. */
  struct semaphore *parent_wait_sema;
  /* Pointer to child process_info. */
  process_info *child_process_info;
  /* Hash elem to be placed into process_info's children hash. */
  struct hash_elem child_elem;

} child_info;

void process_acquire_filesys_lock (void);
void process_release_filesys_lock (void);
int process_add_file (struct file *);
struct file* process_fetch_file (int fd);
struct file* process_remove_file (int fd);

void process_info_init (void);
process_info *process_create_process_info (void);
tid_t process_execute (const char *file_name);
pid_t process_execute_pid (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

process_info *process_current (void);
process_info *process_get_info (pid_t pid);

#endif /* userprog/process.h */
