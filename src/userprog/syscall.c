#include <stdio.h>
#include <syscall-nr.h>
#include <user/syscall.h>

#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "devices/shutdown.h"

#include "userprog/syscall.h"

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
static void *check_pointer (void *);
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
  uint32_t call_no = *(uint32_t*) check_pointer (frame->esp);

  printf ("system call %d!\n", call_no);
  /* TODO: remove above debug print before submission */

  switch (call_no)
  {
    case (SYS_HALT):
      call_syscall_0_void(syscall_halt);
      break;
    case (SYS_EXIT):
      call_syscall_1_void(syscall_exit, frame, int);
      break;
    case (SYS_EXEC):
      frame->eax = call_syscall_1(syscall_exec, pid_t, frame, const char*);
      break;
    case (SYS_WAIT):
      frame->eax = call_syscall_1(syscall_wait, int, frame, pid_t);
      break;
    case (SYS_CREATE):
      frame->eax = call_syscall_2(syscall_create, bool, frame, const char*, unsigned);
      break;
    case (SYS_REMOVE):
      frame->eax = call_syscall_1(syscall_remove, bool, frame, const char*);
      break;
    case (SYS_OPEN):
      frame->eax = call_syscall_1(syscall_open, int, frame, const char*);
      break;
    case (SYS_FILESIZE):
      frame->eax = call_syscall_1(syscall_filesize, int, frame, int);
      break;
    case (SYS_READ):
      frame->eax = call_syscall_3(syscall_read, int, frame, int, void*, unsigned);
      break;
    case (SYS_WRITE):
      frame->eax = call_syscall_3 (syscall_write, uint32_t, frame,
                                   int, const void*, unsigned);
      break;
    case (SYS_SEEK):
      call_syscall_2_void(syscall_seek, frame, int, unsigned);
      break;
    case (SYS_TELL):
      frame->eax = call_syscall_1(syscall_tell, unsigned, frame, int);
      break;
    case (SYS_CLOSE):
      call_syscall_1_void(syscall_close, frame, int);
      break;
    default:
      /* Unknown system call encountered! */
      printf ("That system call is not of this world!\n");
      /* TODO: remove above debug print before submission */
  }

}

/* Checks whether a given pointer is safe to deference, i.e. whether it lies
   below PHYS_BASE and points to mapped user virtual memory. */
static void *
check_pointer (void *uaddr)
{ /* TODO: May need modification to check a range of addresses*/
  if (is_user_vaddr (uaddr)
      && pagedir_get_page (thread_current ()->pagedir, uaddr))
    { /* uaddr is safe (points to mapped user virtual memory). */
      return uaddr;
    }
  else
    { /* uaddr is unsafe. */
      printf ("check_pointer() detected an unsafe address!\n");
      /* TODO: remove above debug print before submission */
      thread_exit ();
      /* Release other syscall-related resources here. */
    }
}

/* Safely dereferences an interrupt frame's stack pointer,
   given an offset. */
static uint32_t
get_arg (struct intr_frame *frame, int offset)
{
  uint32_t *arg_pointer = (((uint32_t*) (frame->esp)) + offset);
  return *((uint32_t*) check_pointer((void*) arg_pointer));
}

/* System call functions below */
/* TODO: Add documentation about what the system call does */

/* Terminates the operating system immediately, without discussion. */
static void
syscall_halt (void)
{
  shutdown_power_off ();
  NOT_REACHED ();
}

static void
syscall_exit (int status)
{
  /* Code here */
  NOT_REACHED ();
}

static pid_t
syscall_exec (const char *file)
{
  return -1;
}

static int
syscall_wait (pid_t pid)
{
  return 0;
}

static bool
syscall_create (const char *file, unsigned initial_size)
{
  return false;
}

static bool
syscall_remove (const char *file)
{
  return false;
}

static int
syscall_open (const char *file)
{
  return 0;
}

static int
syscall_filesize (int fd)
{
  return 0;
}

static int
syscall_read (int fd, void *buffer, unsigned size)
{
  return 0;
}

static int
syscall_write (int fd, const void *buffer, unsigned size)
{
  return 0;
}

static void
syscall_seek (int fd, unsigned position)
{

}

static unsigned
syscall_tell (int fd)
{
  return 0;
}

static void
syscall_close (int fd)
{

}
