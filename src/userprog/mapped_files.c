#include <stdbool.h>
#include <stdio.h>

#include "threads/malloc.h"
#include "userprog/mapped_files.h"
#include "userprog/process.h"
#include "filesys/file.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

mapid_t
syscall_mmap (int fd, void *addr)
{
  if (fd == STDIN || fd == STDOUT)
    {
      return MAP_FAILED;
    }

	struct thread *t = thread_current ();
	struct file *file = file_reopen (process_fetch_file (fd));
  if (file == NULL)
    {
      return MAP_FAILED;
    }

	off_t size = file_length (file);
	if (size == 0 || (int) addr == 0 || (int) addr % PGSIZE != 0)
    {
      return MAP_FAILED;
    }

  int num_of_pages = (size % PGSIZE != 0) ?
    size / PGSIZE + 1 :
    size / PGSIZE;

  int index = 0;
  while (index != num_of_pages)
    {
      if (supp_page_lookup_segment(&t->supp_page_table, addr + index*PGSIZE) != NULL)
        {
          return MAP_FAILED;
        }
    }

  int size_data = num_of_pages * PGSIZE;

  process_info *process = process_current ();
  int id = process->mapid_counter;
  process->mapid_counter++;
  struct hash *mapped_files = &process->mapped_files;
  struct mapid *mapid = try_calloc (1, sizeof *mapid);

  bool writable = true;
  struct supp_page_segment *segment =
    supp_page_create_segment (&t->supp_page_table, addr, writable, size_data);
  supp_page_set_file_data (segment, file, 0, size, true);

  mapid->mapid = id;
	mapid->file = file;
  mapid->segment = segment;
  hash_insert (mapped_files, &mapid->elem);
  return id;
}

void
syscall_munmap (mapid_t mapping)
{
  process_info *process = process_current ();
  struct mapid mapid;
  struct hash_elem *e;

  mapid.mapid = mapping;
  e = hash_find (&process->mapped_files, &mapid.elem);

  struct mapid *actual_mapid = hash_entry (e, struct mapid, elem);
  hash_delete (&process->mapped_files, &mapid.elem);
  supp_page_free_segment (actual_mapid->segment, thread_current ()->pagedir);
  file_close (actual_mapid->file);
  free (actual_mapid);
}


unsigned
mapid_hash_func (const struct hash_elem *e, void *aux UNUSED)
{
  return (unsigned) hash_entry (e, struct mapid, elem)->mapid;
}

bool
mapid_less_func (const struct hash_elem *a,
              const struct hash_elem *b,
              void *aux UNUSED)
{
  return hash_entry (a, struct mapid, elem)->mapid
         < hash_entry (b, struct mapid, elem)->mapid;
}
//Not sure about this yet.
void
mapid_hash_destroy (struct hash_elem *e, void *aux UNUSED)
{
  struct mapid *mapid = hash_entry (e, struct mapid, elem);
  free (mapid);
}
