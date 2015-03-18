#ifndef FILESYS_FILESYS_LOCK_H
#define FILESYS_FILESYS_LOCK_H

void filesys_lock_init (void);
void filesys_lock_acquire (void);
void filesys_lock_release (void);

#endif
