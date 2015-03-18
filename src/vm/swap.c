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

const uint32_t SECTORS_PER_PAGE = PGSIZE / BLOCK_SECTOR_SIZE;
struct list free_slot_list;
struct block *swap_block;

static slot_no get_next_free_page (void);
// static void free_page_slot (slot_no);
static bool range_lt (const struct list_elem *new_elem,
                      const struct list_elem *ori_elem,
                      void *aux UNUSED);

/* Initialise swap table. N.B. This assumes pages divide evenly into pages. */
void
swap_init (void)
{
  list_init (&free_slot_list);
  swap_block = block_get_role (BLOCK_SWAP);

  /* Create initial free range and set to whole of swap space. */
  struct range *initial_range = malloc (sizeof *initial_range);
  initial_range->start = 0;
  initial_range->end = block_size (swap_block) / SECTORS_PER_PAGE;
  list_insert_ordered (&free_slot_list, &initial_range->elem, range_lt, NULL);
}

/* Writes a page to the swap file. */
slot_no
swap_write (void *kpage)
{
  uint8_t *buffer = kpage;
  slot_no slot = get_next_free_page ();
  block_sector_t sector_no = slot * SECTORS_PER_PAGE;

  int i;
  for (i = 0; i < BLOCK_SECTOR_SIZE; i++)
    {
      block_write (swap_block, (sector_no + i), (buffer+i));
    }
  return slot;
}

// /* Retrieves a page from swap by copying the page at slot_no into page. Also
//    frees the slot. */
// void swap_retrieve (slot_no slot, void *kpage)
// {

// }

// /* Marks a page as free in free_slot_list. */
// void swap_free_page (slot_no slot)
// {

// }

// /* Frees all the swap slots occupied by the array of pages passed in. */
// void
// free_used_frames (void) // ARG HAXX
// {
//   return NULL; // RETURN HAXX
// }

static slot_no
get_next_free_page (void)
{
  struct list_elem *free_range_elem = list_begin (&free_slot_list);
  if (free_range_elem == list_tail (&free_slot_list))
    {
      /* Out of swap file space! Panic! */
      PANIC ("Out of swap space!");
    }

  struct range *free_range = list_entry (free_range_elem, struct range, elem);
  int ret = free_range->start;
  free_range->start++;

  /* Remove the range if it's empty. */
  if (free_range->start == free_range->end) {
    list_remove (&free_range->elem);
  }
  return ret;
}

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