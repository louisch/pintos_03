#include <stdbool.h>

#include "userprog/mapped_files.h"
#include "userprog/process.h"
#include "filesys/file.h"
#include "threads/thread.h"



mapid_t
syscall_mmap (int fd, void *addr)
{
	struct thread *t = thread_current ();
	struct file *file = process_fetch_file (fd);

  if (fd == STDIN || fd == STDOUT)
    return -1;

	off_t size = file_length (file);
	if (size == 0 || size == NULL)
		return -1;
  if ((int) addr == 0)
    return -1;
	if ((int) addr % PGSIZE != 0)
		return -1;

	int num_of_pages = (size % PGSIZE != 0) ? 
					   size / PGSIZE + 1 : 
					   size / PGSIZE;
	struct supp_page_entry buffer [num_of_pages ];
	buffer = supp_page_lookup_range (&t->supp_page_table, addr, buffer, num_of_pages );
	int index = 0;
  while (index != num_of_pages )
		{
			if (buffer[index] == NULL)
				{
					return -1;
				}
      index++;
		}

  process_info *process = process_current ();
  int id = process->mapid++;
  struct hash *mapped_files = &process->mapped_files;
  struct mapid *mapid = malloc (sizeof(struct mapid));
  if (mapid == NULL)
    return ABNORMAL_EXIT_STATUS;

  bool writable = TRUE;
  int index = 0
  int offset = 0;
  size_t remaining_bytes = size;
  while (index != num_of_pages)
    {
      offset = index * PGSIZE
      struct supp_page entry *entry = 
        supp_page_create_entry (&t->supp_page_table, addr + offset, writable);
      supp_page_set_file_data (entry, file, offset, 
        remaining_bytes - (num_of_pages - index + 1)*PGSIZE);
      index++;
    }
	mapid->mapid = id;
	mapid->file = file;
	hash_insert (mapped_files, &mapid->elem);
	return id;
}


static unsigned
mapid_hash_func (const struct hash_elem *e void *aux UNUSED)
{
  return (unsigned) hash_entry (e, struct map_id)->mapid
}

static unsigned
mapid_less_func (const struct hash_elem *a,
              const struct hash_elem *b,
              void *aux UNUSED)
{
  return hash_entry (a, struct mapid, elem)->fd
         < hash_entry (b, struct mapid, elem)->fd;
}
//Not sure about this yet.
static void
mapid_hash_destroy (struct hash_elem *e void *aux UNUSED)
{
  struct mapid *mapid = hash_entry (e, struct mapid, elem);
}