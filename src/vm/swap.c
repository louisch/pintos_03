#include <list.h>
#include <threads/malloc.h>

#include "devices/block.h"
#include "swap.h"
#include "threads/vaddr.h"

/* Struct inserted into the list of free ranges,
   marks start (inclusive) and end of range (exclusive). */
struct range
  {
    slot_no start; /* Number of first free swap slot. */
    slot_no end; /* End of range (exclusive i.e. a used slot). */
    struct list_elem elem;
  };

struct list free_slot_list;
struct block *swap_block;

// static slot_no get_next_free_page (void);
// static void free_page_slot (slot_no);
static bool range_lt (const struct list_elem *new_elem,
                      const struct list_elem *ori_elem,
                      void *aux UNUSED);

/* Initialise swap table. */
void
swap_init (void)
{
  list_init (&free_slot_list);
  swap_block = block_get_role (BLOCK_SWAP);

  /* Create initial free range and set to whole of swap space. */
  struct range *init_range = malloc (sizeof *init_range);
  init_range->start = 0;
  init_range->end = block_size (swap_block) / PGSIZE;
  list_insert_ordered (&free_slot_list, &init_range->elem, range_lt, NULL);
}

// /* Insert a page into the swap table. */
// void
// swap_insert (void *kpage)
// {
//   get_next_free_page ();
// }

// /* Frees all the swap slots occupied by the array of pages passed in. */
// void
// free_used_frames (void) // ARG HAXX
// {
//   return NULL; // RETURN HAXX
// }

// static slot_no
// get_next_free_page (void)
// {
//   return NULL; // MOAR RETURN HAXX
// }

// /* Frees a page slot belonging to a file and combines ranges when necessary. */
// static void
// free_page_slot (slot_no slot) // ARG(H) HAXX
// {
//   return NULL; // RETURN HAXX
// }

/* list_less_func for comparing thread priority. Note that here, A
   is the element to be inserted and B is the element in the list. */
static bool
range_lt (const struct list_elem *new_elem,
          const struct list_elem *ori_elem,
          void *aux UNUSED)
{
  int new = (list_entry (new_elem, struct range, elem))->start;
  int ori = (list_entry (ori_elem, struct range, elem))->start;

  return new < ori;
}