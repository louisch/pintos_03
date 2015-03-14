#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include <kernel/list.h>
#include <kernel/hash.h>
#include <user/syscall.h>
#include "filesys/file.h"
#include "threads/synch.h"

/* Data for a process used for syscalls. */
typedef struct process_info
{
  /* Process ID. */
  pid_t pid;

  /* Exit status of process. */
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

  /* Cond to notify parent process that child process is finished loading. */
  struct condition finish_load;
  /* Pointer to lock held by parent. */
  struct lock *reply_lock;

  /* Provides unique files descriptors for process. */
  unsigned fd_counter;
  /* Hash used to remember files open by process by their fd. */
  struct hash open_files;

} process_info;

/* Data about a process's child, carried by the parent process_info. */
typedef struct child_info
{
  /* Child process's ID, used when checking if a child belongs to a parent. */
  pid_t pid;
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
  /* Lock to synchronise reads/writes to the child_info's fields. */
  struct lock child_lock;

} child_info;

process_info *process_execute_aux (const char *file_name, struct lock *lock);

void process_acquire_filesys_lock (void);
void process_release_filesys_lock (void);
int process_add_file (struct file *);
struct file* process_fetch_file (int fd);
struct file* process_remove_file (int fd);

void process_info_init (void);
process_info *process_create_process_info (void);
pid_t process_execute_pid (const char *file_name);
int process_wait (pid_t);
void process_exit (void);
void process_activate (void);

process_info *process_current (void);

#endif /* userprog/process.h */
