#include <list.h>
#include <threads/malloc.h>

#include "devices/block.h"
#include "swap.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

#define block_do(KPAGE, SLOT, FUNC)              \
  int i;                                         \
  for (i = 0; i < BLOCK_SECTOR_SIZE; i++)        \
    {                                            \
      FUNC (swap_block,                          \
            (convert_slot_to_sector (SLOT) + i), \
            (((uint8_t*) KPAGE) + i));           \
    }

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
struct lock free_slot_list_lock;

static block_sector_t convert_slot_to_sector (slot_no);
static slot_no get_next_free_page (void);
// static void free_page_slot (slot_no);
static bool range_lt (const struct list_elem *new_elem,
                      const struct list_elem *ori_elem,
                      void *aux UNUSED);

/* Initialise swap table. N.B. This assumes pages divide evenly into sectors. */
void
swap_init (void)
{
  list_init (&free_slot_list);
  swap_block = block_get_role (BLOCK_SWAP);
  lock_init (&free_slot_list_lock);

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
  slot_no slot = get_next_free_page ();
  block_do (kpage, slot, block_write);
  return slot;
}

/* Retrieves a page from swap by copying the page at slot_no into page. Also
   frees the slot. */
void swap_retrieve (slot_no slot, void *kpage)
{
  block_do (kpage, slot, block_read);
  swap_free_slot (slot);
}

/* Reinserts a free slot into free_slot_list.
   May create, extend or merge ranges. */
void swap_free_slot (slot_no slot)
{
  lock_acquire (&free_slot_list_lock);
  // slot_no start = slot;
  // slot_no end  = slot + 1;

  // TERRIBLE EFFICIENCY HAXX
  struct range *new_range = malloc (sizeof *new_range);
  new_range->start = slot;
  new_range->end = slot + 1;
  list_insert_ordered (&free_slot_list, &new_range->elem, range_lt, NULL);

  /* Iterate through free_slot_list and find the immediately smaller range. */
  // struct list_elem *e;
  // for (e = list_begin(&free_slot_list), e != list_end)
  //   {

  //   }

  lock_release (&free_slot_list_lock);
}

// /* Frees all the swap slots occupied by the array of pages passed in. */
// void
// free_used_frames (void) // ARG HAXX
// {
//   return NULL; // RETURN HAXX
// }

static block_sector_t
convert_slot_to_sector (slot_no slot)
{
  return slot * SECTORS_PER_PAGE;
}

static slot_no
get_next_free_page (void)
{
  lock_acquire (&free_slot_list_lock);
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

  lock_release (&free_slot_list_lock);
  return ret;
}

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