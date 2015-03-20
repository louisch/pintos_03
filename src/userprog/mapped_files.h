#ifndef USERPROG_MAPPED_FILES_H
#define USERPROG_MAPPED_FILES_H

#include <user/syscall.h>
#include <kernel/hash.h>

mapid_t syscall_mmap (int fd, void *addr);
void syscall_munmap (mapid_t mapid);


struct mapid
  {
    mapid_t mapid;
    struct file *file;
    struct supp_page_segment *segment;
    struct hash_elem elem;
  };

/* Mapid hash related functions */
unsigned mapid_hash_func (const struct hash_elem *e, void *aux UNUSED);
void mapid_hash_destroy (struct hash_elem *e, void *aux UNUSED);
bool mapid_less_func (const struct hash_elem *a,
                          const struct hash_elem *b,
                          void *aux UNUSED);
void munmap (mapid_t mapping);
#endif
