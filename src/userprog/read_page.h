#ifndef USERPROG_READ_PAGE_H
#define USERPROG_READ_PAGE_H

#include <stddef.h>
#include <filesys/file.h>

bool read_page (void *kpage, struct file *file,
                size_t page_read_bytes, size_t read_zero_bytes);

#endif /* userprog/read_page.h */
