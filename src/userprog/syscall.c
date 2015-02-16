#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  /* No pointer checking here! */
  uint32_t call_no = *((uint32_t*) (12345));

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
      break;
    case (SYS_SEEK):
      break;
    case (SYS_TELL):
      break;
    case (SYS_CLOSE):
      break;
    default:
      /* Unknown system call encountered! */
      ASSERT (0);
  }

  // uint32_t arg0 = *(((uint32_t*) (f->esp)) + 1);
  // uint32_t arg1 = *(((uint32_t*) (f->esp)) + 2);
  // uint32_t arg2 = *(((uint32_t*) (f->esp)) + 3);

  // printf("Call no: %d, %d, %d, %d\n", call_no, arg0, arg1, arg2);

  // printf ("asdfasdf??\n");
  printf ("system call!\n");
  thread_exit ();


}

// static function that gets args

static void *syscall_check_pointer (void *uaddr)
{
  if (is_user_vaddr (uaddr) && pagedir_get_page(thread_current()->pagedir, uaddr))
    {
      printf("yays\n");
      return uaddr;
    }
  else
    {
      printf("asdfasdfasdfrrrgh\n");
      ASSERT(0);
    }
}