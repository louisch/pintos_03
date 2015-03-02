#include <stdio.h>
#include <syscall-nr.h>
#include <user/syscall.h>

#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "devices/shutdown.h"

#include "userprog/syscall.h"
#include "lib/hash_f.h"

/* Syscall required imports. */
/* halt */
#include "devices/shutdown.h"
/* write */
#include "kernel/stdio.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "userprog/process.h"

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
static const void *check_pointer (const void *uaddr, unsigned size);
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

static void
syscall_handler (struct intr_frame *frame)
{
  uint32_t call_no = *(uint32_t*) check_pointer (frame->esp, 1);

  // printf ("system call %d!\n", call_no);
  /* TODO: remove above debug print before submission */

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
        frame->eax = call_syscall_2 (syscall_create, bool, frame, const char*, unsigned);
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
        frame->eax = call_syscall_3 (syscall_read, int, frame, int, void*, unsigned);
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
    default:
        /* Unknown system call encountered! */
        printf ("That system call is not of this world!\n");
          /* TODO: remove above debug print before submission */
  }

}

/* Checks whether a given range in memory (starting from uaddr to uaddr + size)
   is safe to deference, i.e. whether it lies below PHYS_BASE and points to
   mapped user virtual memory. */
static const void *
check_pointer (const void *uaddr, size_t size)
{
  if (uaddr == NULL)
    thread_exit ();
  const uint8_t *start = (uint8_t *) uaddr;
  const uint8_t *pos;
  for (pos = uaddr; pos >= start && pos < size + start; pos++)
    {
      if (!(is_user_vaddr (pos)
          && (pagedir_get_page (thread_current ()->pagedir, pos) != NULL)))
        {
          /* uaddr is unsafe. */
          thread_exit ();
          /* Release other syscall-related resources here. */
        }
    }
  /* uaddr is safe (points to mapped user virtual memory). */
  return uaddr;
}

/* Safely dereferences an interrupt frame's stack pointer,
   given an offset and a size of bytes. */
static uint32_t
get_arg (struct intr_frame *frame, int offset)
{
  uint32_t *arg_pointer = (((uint32_t*) (frame->esp)) + offset);
  return *((uint32_t*) check_pointer ((void*) arg_pointer, 4));
}

/* System call functions below */
/* TODO: Add documentation about what the system call does */

/* Terminates the operating system immediately, without discussion. */
static void
syscall_halt (void)
{
  process_info_kill_all ();
  shutdown_power_off ();
  NOT_REACHED ();
}

static void
syscall_exit (int status)
{
  process_current ()->exit_status = status;
  thread_exit ();
  NOT_REACHED ();
}

static pid_t
syscall_exec (const char *cmd_line UNUSED)
{
  process_info *info = process_execute_aux (cmd_line);

  return info->pid;
}

static int
syscall_wait (pid_t pid UNUSED)
{
  return 0;
}

/* Creates file of size initial_size bytes with given name.
   Returns true upon success, false otherwise. */
static bool
syscall_create (const char *file, unsigned initial_size)
{
  check_pointer (file, initial_size);
  bool success = false;
  process_acquire_filesys_lock ();
  success = filesys_create (file, initial_size);
  process_release_filesys_lock ();
  return success;
}

/* Removes a file from the file system.
   Returns true upon success, false on failure. */
static bool
syscall_remove (const char *file UNUSED)
{
  bool success = false;
  process_acquire_filesys_lock ();
  success = filesys_remove (file);
  process_release_filesys_lock ();
  return success;
}

/* Opens file with given name.
   Returns -1 if file is not found. */
static int
syscall_open (const char *file)
{
  process_acquire_filesys_lock ();
  struct file *open_file = filesys_open (file);
  if (open_file == NULL) /* File not found. */
    {
      return -1;
    }
  process_release_filesys_lock ();
  return process_add_file (open_file);
}

/* Returns the size of the file in bytes,
   -1 if file is not open in the process. */
static int
syscall_filesize (int fd)
{
  int size = -1; /* File not found default value. */
  process_acquire_filesys_lock ();
  struct file *file = process_fetch_file (fd);
  if (file != NULL) /* File found. */
    {
      size = file_length (file);
    }

  process_release_filesys_lock ();
  return size;
}

/* Opens file at fd and reads from position.
   Returns the number of bytes read, 0 if no file is found. */
static int
syscall_read (int fd, void *buffer, unsigned size)
{
  process_acquire_filesys_lock ();
  struct file *file = process_fetch_file (fd);
  if (file == NULL) /* File not found. */
    {
      return -1;
    }
  process_release_filesys_lock ();
  return file_read_at (file, buffer, size, file_tell (file));
}

/* Max buffer size for reasonable console writes. */
static unsigned console_write_size = 256;

/*
  Writes size bytes from buffer to open file fd.
  Returns number of bytes writen.

  Does not extend files past their size.
  If fd = 1, writes to console with putbuf.
*/
static int
syscall_write (int fd, const void *buffer, unsigned size)
{
  if (fd < 1) return 0; /* Quit if fd cannot be written to. */
  int written;

  if (fd == 1)
    {
      char *buff = (char *) buffer;
      while (size > 0)
        { /* Writes to console buffer in chunks of console_write_size bytes. */
          unsigned write = size >= console_write_size ? console_write_size : size;
          putbuf (buff, write);
          buff += write;
          size -= write;
        }
      written = (int) size;
    }
  else
    {
      process_acquire_filesys_lock ();
      struct file *file = process_fetch_file (fd);
      if (file == NULL) /* File not found. */
        {
          return 0;
        }
      written = file_write_at (file, buffer, size, file_tell (file));
      process_release_filesys_lock ();
    }

  return written;
}

/* Changes the next byte to be read or written in open file fd to position,
   expressed in bytes from the beginning of the file. */
static void
syscall_seek (int fd, unsigned position)
{
  process_acquire_filesys_lock ();
  struct file *file = process_fetch_file (fd);
  if (file != NULL) /* File found. */
    file_seek (file, position);
  process_release_filesys_lock ();
}

/* Returns the position of the next byte to be read or written in open file fd,
   expressed in bytes from the beginning of the file.
   Returns -1 if no fd is not a valid file descriptor. */
static unsigned
syscall_tell (int fd)
{
  unsigned pos = -1; /* Default file-not-found position. */
  process_acquire_filesys_lock ();
  struct file *file = process_fetch_file (fd);
  if (file != NULL) /* File found. */
    {
      pos = file_tell (file);
    }
  process_release_filesys_lock ();
  return pos;
}

/* Closes file descriptor fd. */
static void
syscall_close (int fd UNUSED)
{
  process_acquire_filesys_lock ();
  struct file *file = process_remove_file (fd);
  if (file != NULL) /* File found. */
    {
      file_close (file);
    }
  process_release_filesys_lock ();
}
