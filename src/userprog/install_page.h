#ifndef USERPROG_INSTALL_PAGE_H
#define USERPROG_INSTALL_PAGE_H

#include <stdbool.h>

bool install_page (void *upage, void *kpage, bool writable);

#endif /* userprog/install_page.h */
