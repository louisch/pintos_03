#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <filesys/file.h>
#include <threads/palloc.h>
#include <threads/vaddr.h>
#ifdef VM
#include <vm/frame.h>
#endif

/* Try to read a page from a file.
   page_read_bytes gives the number of bytes to read from the file, and
   page_zero_bytes gives the number of bytes to zero out after the bytes
   read from the file.
   Hence, page_read_bytes and page_zero_bytes must sum to PGSIZE.
   Returns true if the read succeeded, false otherwise. */
bool
read_page (void *kpage, struct file *file,
           size_t page_read_bytes, size_t page_zero_bytes)
{
  ASSERT (page_read_bytes + page_zero_bytes == PGSIZE);

  if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
    {
#ifndef VM
      palloc_free_page (kpage);
#else
      free_frame (kpage);
#endif
      return false;
    }
  memset (kpage + page_read_bytes, 0, page_zero_bytes);
  return true;
}
