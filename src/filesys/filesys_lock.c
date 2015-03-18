#include <threads/synch.h>

#include <filesys/filesys_lock.h>

/* Lock used to synchronise filesystem operations in process.c and syscall.c. */
static struct lock filesys_access;

/* Initializes the process_info system. */
void
filesys_lock_init (void)
{
  lock_init (&filesys_access);
}

/* Acquires lock over filesystem. */
void
filesys_lock_acquire (void)
{
  lock_acquire (&filesys_access);
}

/* Releases lock over filesystem. */
void
filesys_lock_release (void)
{
  lock_release (&filesys_access);
}
