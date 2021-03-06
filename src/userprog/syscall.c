#include <stdio.h>
#include <syscall-nr.h>
#include <user/syscall.h>

#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "devices/shutdown.h"

/* Syscall required imports. */
#include "filesys/filesys_lock.h"
/* halt */
#include "devices/shutdown.h"
/* write */
#include "kernel/stdio.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "userprog/process.h"

#include "userprog/syscall.h"


#ifdef VM
#include <vm/mapped_files.h>
#include <vm/supp_page.h>
#endif

#define ABNORMAL_IO_VALUE -1

/* Max buffer size for reasonable console writes. */
static const unsigned CONSOLE_WRITE_SIZE = 256;

#define call_syscall_0_void(FUNC)                             \
  FUNC ()

#define call_syscall_1_void(FUNC, FRAME, ARG1)                \
  FUNC ((ARG1) get_arg (FRAME, 1))

#define call_syscall_2_void(FUNC, FRAME, ARG1, ARG2)          \
  FUNC ((ARG1) get_arg (FRAME, 1),                            \
        (ARG2) get_arg (FRAME, 2))

#define call_syscall_1(FUNC, RETURN, FRAME, ARG1)             \
  (RETURN) FUNC ((ARG1) get_arg (FRAME, 1))

#define call_syscall_2(FUNC, RETURN, FRAME, ARG1, ARG2)       \
  (RETURN) FUNC ((ARG1) get_arg (FRAME, 1),                   \
                 (ARG2) get_arg (FRAME, 2))

#define call_syscall_3(FUNC, RETURN, FRAME, ARG1, ARG2, ARG3) \
  (RETURN) FUNC ((ARG1) get_arg (FRAME, 1),                   \
                 (ARG2) get_arg (FRAME, 2),                   \
                 (ARG3) get_arg (FRAME, 3))

static void syscall_handler (struct intr_frame *);
static const void *check_pointer (const uint32_t *uaddr, unsigned size);
static const char *check_filename (const char *filename);
static uint32_t get_arg (struct intr_frame *f, int offset);

static void syscall_halt (void) NO_RETURN;
static void syscall_exit (int status) NO_RETURN;
static pid_t syscall_exec (const char *file);
static int syscall_wait (pid_t);
static bool syscall_create (const char *file, unsigned initial_size);
static bool syscall_remove (const char *file);
static int syscall_open (const char *file);
static int syscall_filesize (int fd);
static int syscall_read (int fd, void *buffer, unsigned length);
static int syscall_write (int fd, const void *buffer, unsigned length);
static void syscall_seek (int fd, unsigned position);
static unsigned syscall_tell (int fd);
static void syscall_close (int fd);



void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* Handles system calls by fetching the call no. off the stack, and then calling
   the appropriate function with the correct arguments (also fetched off the
   stack). */
static void
syscall_handler (struct intr_frame *frame)
{
  uint32_t call_no = *(uint32_t*) check_pointer (frame->esp, 1);

  switch (call_no)
  {
  case (SYS_HALT):
    call_syscall_0_void (syscall_halt);
    break;
  case (SYS_EXIT):
    call_syscall_1_void (syscall_exit, frame, int);
    break;
  case (SYS_EXEC):
    frame->eax = call_syscall_1 (syscall_exec, pid_t, frame, const char*);
    break;
  case (SYS_WAIT):
    frame->eax = call_syscall_1 (syscall_wait, int, frame, pid_t);
    break;
  case (SYS_CREATE):
    frame->eax = call_syscall_2 (syscall_create, bool, frame,
                                 const char*, unsigned);
    break;
  case (SYS_REMOVE):
    frame->eax = call_syscall_1 (syscall_remove, bool, frame, const char*);
    break;
  case (SYS_OPEN):
    frame->eax = call_syscall_1 (syscall_open, int, frame, const char*);
    break;
  case (SYS_FILESIZE):
    frame->eax = call_syscall_1 (syscall_filesize, int, frame, int);
    break;
  case (SYS_READ):
    frame->eax = call_syscall_3 (syscall_read, int, frame,
                                 int, void*, unsigned);
    break;
  case (SYS_WRITE):
    frame->eax = call_syscall_3 (syscall_write, uint32_t, frame,
                                 int, const void*, unsigned);
    break;
  case (SYS_SEEK):
    call_syscall_2_void (syscall_seek, frame, int, unsigned);
    break;
  case (SYS_TELL):
    frame->eax = call_syscall_1 (syscall_tell, unsigned, frame, int);
    break;
  case (SYS_CLOSE):
    call_syscall_1_void (syscall_close, frame, int);
    break;
#ifdef VM
  case (SYS_MMAP):
    frame->eax = call_syscall_2 (syscall_mmap, mapid_t, frame,
                                  int, void*);
    break;
  case (SYS_MUNMAP):
    call_syscall_1_void (syscall_munmap, frame, mapid_t);
    break;
#endif
  default:
    /* Unknown system call encountered! */
    printf ("That system call is not of this world!\n");
  }

}

/* Checks whether a given range in memory (starting from uaddr to uaddr+size-1)
   is safe to deference, i.e. whether it lies below PHYS_BASE and points to
   mapped user virtual memory. */
static const void *
check_pointer (const uint32_t *uaddr, size_t size)
{
  if (uaddr == NULL)
    {
      thread_exit ();
    }
  if (size == 0)
    {
      return uaddr;
    }
  uint8_t *start = (uint8_t *) uaddr;
  uint8_t *next = start;
  for (; next < start + size; ++next)
    {
#ifndef VM
      if (!is_user_vaddr (start)
          || (pagedir_get_page (thread_current ()->pagedir, start) == NULL))
        {
          /* uaddr is unsafe. */
          thread_exit ();
          /* Release other syscall-related resources here. */
        }
#else
      if (!is_user_vaddr (start)
          || (supp_page_lookup_segment (&thread_current ()->supp_page_table,
                                        start) == NULL))
        {
          thread_exit ();
        }
#endif
    }
  /* uaddr is safe (points to mapped user virtual memory). */
  return uaddr;
}

/* Verifies that the given filename points to a valid space in memory. */
static const char *
check_filename (const char *filename)
{
  return (const char *) check_pointer ((const uint32_t *)filename,
                                       READDIR_MAX_LEN);
}

/* Safely dereferences an interrupt frame's stack pointer,
   given an offset and a size of bytes. */
static uint32_t
get_arg (struct intr_frame *frame, int offset)
{
  uint32_t *arg_pointer = (((uint32_t*) (frame->esp)) + offset);
  return *((uint32_t*) check_pointer (arg_pointer, 4));
}

/* System call functions below */

/* Terminates the operating system immediately, without discussion. */
static void
syscall_halt (void)
{
  shutdown_power_off ();
  NOT_REACHED ();
}

/* Terminates the current process with the supplied exit status and frees its
   resources. */
static void
syscall_exit (int status)
{
  process_current ()->persistent->exit_status = status;
  thread_exit ();
  NOT_REACHED ();
}

/* Spawns a process from the given cmd input.
   Waits for the new process to load and returns pid. */
static pid_t
syscall_exec (const char *cmd_line)
{
  if (check_pointer ((const void *)cmd_line, 1) == NULL)
    {
      return PID_ERROR;
    }

  persistent_info *c_info = process_execute_aux (cmd_line);

  /* Note that child info persists even if child process already exited. */
  return c_info->pid;
}

/* Waits on a process to exit and returns its thread's exit status. If it has
   already exited, returns immediately. */
static int
syscall_wait (pid_t pid)
{
  return process_wait (pid);
}

/* Creates file of size initial_size bytes with given name.
   Returns true upon success, false otherwise. */
static bool
syscall_create (const char *file, unsigned initial_size)
{
  check_filename (file);
  bool success = false;
  filesys_lock_acquire ();
  success = filesys_create (file, initial_size);
  filesys_lock_release ();
  return success;
}

/* Removes a file from the file system.
   Returns true upon success, false on failure. */
static bool
syscall_remove (const char *file)
{
  check_filename (file);
  bool success = false;
  filesys_lock_acquire ();
  success = filesys_remove (file);
  filesys_lock_release ();
  return success;
}

/* Opens file with given name.
   Returns ABNORMAL_IO_VALUE if file is not found. */
static int
syscall_open (const char *file)
{
  check_filename (file);
  filesys_lock_acquire ();
  struct file *open_file = filesys_open (file);
  filesys_lock_release ();
  if (open_file == NULL) /* File not found. */
    {
      return ABNORMAL_IO_VALUE;
    }
  return process_add_file (open_file);
}

/* Returns the size of the file in bytes,
   ABNORMAL_IO_VALUE if file is not open in the process. */
static int
syscall_filesize (int fd)
{
  int size = ABNORMAL_IO_VALUE; /* File not found default value. */
  filesys_lock_acquire ();
  struct file *file = process_fetch_file (fd);
  if (file != NULL) /* File found. */
    {
      size = file_length (file);
    }

  filesys_lock_release ();
  return size;
}

/* Opens file at fd and reads from position.
   Returns the number of bytes read, 0 if no file is found. */
static int
syscall_read (int fd, void *buffer, unsigned size)
{
  check_pointer (buffer, size);
  int ret = ABNORMAL_IO_VALUE;
  if (fd == STDIN || fd == STDOUT || fd < 0)
    {
      return ret; /* Bad fd. */
    }

  filesys_lock_acquire ();
  struct file *file = process_fetch_file (fd);
  if (file != NULL) /* File not found. */
    {
      ret = file_read_at (file, buffer, size, file_tell (file));
      file_seek (file, file_tell (file) + ret);
    }
  filesys_lock_release ();
  return ret;
}

/*
  Writes size bytes from buffer to open file fd.
  Returns number of bytes written.

  Does not extend files past their size.
  If fd = 1, writes to console with putbuf.
*/
static int
syscall_write (int fd, const void *buffer, unsigned size)
{
  if (fd == STDIN || fd < 0) return 0; /* Bad fd. */
  check_pointer (buffer, size);
  int written;

  if (fd == STDOUT)
    {
      char *buff = (char *) buffer;
      while (size > 0)
        { /* Writes to console buffer in chunks of CONSOLE_WRITE_SIZE bytes. */
          unsigned write = size >= CONSOLE_WRITE_SIZE ? CONSOLE_WRITE_SIZE : size;
          putbuf (buff, write);
          buff += write;
          size -= write;
        }
      written = (int) size;
    }
  else
    {
      struct file *file = process_fetch_file (fd);
      if (file == NULL) /* File not found. */
        {
          return ABNORMAL_IO_VALUE;
        }
      filesys_lock_acquire ();
      written = file_write_at (file, buffer, size, file_tell (file));
      file_seek (file, file_tell (file) + written);
      filesys_lock_release ();
    }

  return written;
}

/* Changes the next byte to be read or written in open file fd to position,
   expressed in bytes from the beginning of the file. */
static void
syscall_seek (int fd, unsigned position)
{
  filesys_lock_acquire ();
  struct file *file = process_fetch_file (fd);
  if (file != NULL) /* File found. */
    {
      file_seek (file, position);
    }
  filesys_lock_release ();
}

/* Returns the position of the next byte to be read or written in open file fd,
   expressed in bytes from the beginning of the file.
   Returns ABNORMAL_IO_VALUE if no fd is not a valid file descriptor. */
static unsigned
syscall_tell (int fd)
{
  unsigned pos = ABNORMAL_IO_VALUE; /* Default file-not-found position. */
  filesys_lock_acquire ();
  struct file *file = process_fetch_file (fd);
  if (file != NULL) /* File found. */
    {
      pos = file_tell (file);
    }
  filesys_lock_release ();
  return pos;
}

/* Closes file descriptor fd. */
static void
syscall_close (int fd)
{
  filesys_lock_acquire ();
  struct file *file = process_remove_file (fd);
  if (file != NULL) /* File found. */
    {
      file_close (file);
    }
  filesys_lock_release ();
}
