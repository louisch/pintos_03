#include <vm/supp_page.h>

#include <debug.h>
#include <stdbool.h>
#include <stdint.h>
#include <lib/kernel/hash.h>

#include <threads/malloc.h>
#include <threads/palloc.h>
#include <userprog/pagedir.h>
#include <vm/frame.h>

static unsigned supp_page_hash_func (const struct hash_elem *e, void *aux UNUSED);
static bool supp_page_less_func (const struct hash_elem *a,
                                 const struct hash_elem *b,
                                 void *aux UNUSED);
static struct supp_page_entry *supp_page_from_elem (const struct hash_elem *e);

/* Initialize the given supplementary page table with the given page table. */
void
supp_page_table_init (struct supp_page_table *supp_page_table)
{
  hash_init (&supp_page_table->table, supp_page_hash_func,
             supp_page_less_func, NULL);
}

/* Allocates a new supplementary page table entry, and insert it into the hash. */
struct supp_page_entry *
create_entry (struct supp_page_table *supp_page_table,
              void *uaddr, bool writable, bool all_zeroes)
{
  struct supp_page_entry *entry = calloc (1, sizeof *entry);
  entry->uaddr = uaddr;
  entry->writable = writable;
  entry->all_zeroes = all_zeroes;

  hash_insert (&supp_page_table->table, &entry->supp_elem);
  return entry;
}

/* Try to get a frame and map entry->upage to this frame in the page table. */
void *
map_user_addr (uint32_t *pd, struct supp_page_entry *entry)
{
  enum palloc_flags flags = PAL_USER;
  if (entry->all_zeroes)
    {
      flags = flags & PAL_ZERO;
    }

  void *kpage = request_frame (flags);
  pagedir_set_page (pd, entry->uaddr, kpage, entry->writable);
  return kpage;
}

/* Used for setting up the hash table. Gets hash value for hash elements. */
static unsigned
supp_page_hash_func (const struct hash_elem *e, void *aux UNUSED)
{
  uint32_t *uaddr = (uint32_t *)supp_page_from_elem (e)->uaddr;
  /* sizeof returns how many bytes uaddr, the pointer itself (not what it is
     pointing to), takes up. */
  return hash_bytes (uaddr, sizeof uaddr);
}

/* Used for setting up hash table. Orders hash elements. */
static bool
supp_page_less_func (const struct hash_elem *a, const struct hash_elem *b,
                     void *aux UNUSED)
{
  uint32_t *upage_a = (uint32_t *)supp_page_from_elem (a)->uaddr;
  uint32_t *upage_b = (uint32_t *)supp_page_from_elem (b)->uaddr;
  return upage_a < upage_b;
}

/* Get the supp_page_entry wrapping a supp_elem. */
static struct supp_page_entry *
supp_page_from_elem (const struct hash_elem *e)
{
  ASSERT (e != NULL);
  return hash_entry (e, struct supp_page_entry, supp_elem);
}
