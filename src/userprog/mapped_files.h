#ifndef USERPROG_MAPPED_FILES_H
#define USERPROG_MAPPED_FILES_H

#include <user/syscall.h>

mapid_t syscall_mmap (int fd, void *addr);
void syscall_munmap (mapid_t mapid);

#endif