#ifndef USERPROG_MAPPED_FILES_H
#define USERPROG_MAPPED_FILES_H

#include <user/syscall.h>

mapid_t syscall_mmap (int fd, void *addr);
void syscall_munmap (mapid_t mapid);


struct mapid
  {
    mapid_t mapidid;
    struct file *file;
    struct hash_elem elem;
  }
  
/* Mapid hash related functions */
static unsigned mapid_hash_func (const struct hash_elem *e void *aux UNUSED);
static void mapid_hash_destroy (struct hash_elem *e, void *aux UNUSED);
static bool mapid_less_func (const struct hash_elem *a,
                          const struct hash_elem *b,
                          void *aux UNUSED);
#endif