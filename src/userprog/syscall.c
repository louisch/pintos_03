#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

#define call_syscall_0(FUNC, RETURN)                          \
  (RETURN) FUNC ()

#define call_syscall_1(FUNC, RETURN, FRAME, ARG1)             \
  (RETURN) FUNC ((ARG1) get_arg (FRAME, 1),                   \
                 (ARG2) get_arg (FRAME, 2))

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
static int write (int fd, const void *buffer, unsigned size);

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
      break;
    case (SYS_EXIT):
      break;
    case (SYS_EXEC):
      break;
    case (SYS_WAIT):
      break;
    case (SYS_CREATE):
      break;
    case (SYS_REMOVE):
      break;
    case (SYS_OPEN):
      break;
    case (SYS_FILESIZE):
      break;
    case (SYS_READ):
      break;
    case (SYS_WRITE):
      frame->eax = call_syscall_3 (write, uint32_t, frame,
                                   int, const void*, unsigned);
      break;
    case (SYS_SEEK):
      break;
    case (SYS_TELL):
      break;
    case (SYS_CLOSE):
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
{
  if (is_user_vaddr (uaddr)
      && pagedir_get_page (thread_current ()->pagedir, uaddr))
    { /* uaddr is safe (points to mapped user virtual memory). */
      return uaddr;
    }
  else
    { /* uaddr is unsafe. */
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



/* write system call. */
static int
write (int fd, const void *buffer, unsigned size)
{
  /* TODO: implement */
  return 0;
}
