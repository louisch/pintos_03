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

enum action_enum
  {
    EXTEND_NONE,
    EXTEND_SMALLER,
    EXTEND_LARGER,
    EXTEND_BOTH
  };

const uint32_t SECTORS_PER_PAGE = PGSIZE / BLOCK_SECTOR_SIZE;
struct list free_slot_list;
struct block *swap_block;
struct lock free_slot_list_lock;

static struct range *range_entry (const struct list_elem *);
static block_sector_t convert_slot_to_sector (slot_no);
static enum action_enum check_extend_smaller (struct list_elem *, slot_no slot);
static enum action_enum check_extend_larger (struct list_elem *, slot_no slot);
static void create_range (slot_no, slot_no);
static void delete_range (struct range *);
static slot_no get_next_free_slot (void);
// static void free_page_slot (slot_no);
static bool range_lt (const struct list_elem *, const struct list_elem *,
                      void *);

/* Initialise swap table. N.B. This assumes pages divide evenly into sectors. */
void
swap_init (void)
{
  list_init (&free_slot_list);
  swap_block = block_get_role (BLOCK_SWAP);
  lock_init (&free_slot_list_lock);

  /* Create initial free range and set to whole of swap space. */
  create_range (0, block_size (swap_block) / SECTORS_PER_PAGE);
}

/* Writes a page to the swap file. */
slot_no
swap_write (void *kpage)
{
  slot_no slot = get_next_free_slot ();
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

  /* Find the first range with a start point greater than slot. */
  struct list_elem *e = list_begin (&free_slot_list);
  while (e != list_end (&free_slot_list))
    {
      struct range *free_range = range_entry (e);
      ASSERT (free_range->start != slot); /* slot should not be free yet! */
      if (free_range->start > slot) {
        break;
      }
      e = list_next (e);
    }

  enum action_enum action = check_extend_smaller (e, slot);
  if (check_extend_larger (e, slot) == EXTEND_LARGER)
    {
      action = action == EXTEND_NONE ? EXTEND_LARGER : EXTEND_BOTH;
    }

  switch (action)
  {
  case (EXTEND_NONE):
    create_range (slot, slot + 1);
    break;
  case (EXTEND_SMALLER):
    range_entry (list_prev (e))->end++;
    break;
  case (EXTEND_LARGER):
    range_entry (e)->start--;
    break;
  case (EXTEND_BOTH):
    range_entry (list_prev (e))->end = range_entry (e)->end;
    delete_range (range_entry (e));
    break;
  }

  lock_release (&free_slot_list_lock);
}

// /* Frees all the swap slots occupied by the array of pages passed in. */
// void
// free_used_frames (void) // ARG HAXX
// {
//   return NULL; // RETURN HAXX
// }

/* Wrapper for the list_entry macro, returning the pointer
   to the struct range which contains the list_elem *e.
   Must hold free_slot_list_lock before calling. */
static struct range *
range_entry (const struct list_elem *e)
{
  ASSERT (lock_held_by_current_thread (&free_slot_list_lock));
  return list_entry (e, struct range, elem);
}

/* Converts a slot_no to a block_sector_t. */
static block_sector_t
convert_slot_to_sector (slot_no slot)
{
  return slot * SECTORS_PER_PAGE;
}

/* Checks whether inserting slot will extend
   the smaller range containing curr_elem. */
static enum action_enum
check_extend_smaller (struct list_elem *curr_elem, slot_no slot)
{
  struct list_elem *prev_elem = list_prev (curr_elem);
  if (prev_elem != list_head (&free_slot_list))
    {
      return range_entry (prev_elem)->end == slot ? EXTEND_SMALLER : EXTEND_NONE;
    }
  else
    {
      return EXTEND_NONE;
    }
}

/* Checks whether inserting slot will extend
   the range containing curr_elem. */
static enum action_enum
check_extend_larger (struct list_elem *curr_elem, slot_no slot)
{
  if (curr_elem != list_head (&free_slot_list))
    {
      return (range_entry (curr_elem)->start - 1) == slot ?
        EXTEND_LARGER : EXTEND_NONE;
    }
  else
    {
      return EXTEND_NONE;
    }
}

/* Allocates memory for a range,
   and inserts it into free_slot_list in the correct place. */
static void
create_range (slot_no start, slot_no end)
{
  lock_acquire (&free_slot_list_lock);

  struct range *new_range = malloc (sizeof *new_range);
  new_range->start = start;
  new_range->end = end;
  list_insert_ordered (&free_slot_list, &new_range->elem, range_lt, NULL);

  lock_release (&free_slot_list_lock);
}

/* Removes a range from free_slot_list and frees its memory. */
static void
delete_range (struct range *range_to_delete)
{
  list_remove (&range_to_delete->elem);
  free (range_to_delete);
}

/* Gets the next available free slot,
   and deletes the range it was in if it is now empty. */
static slot_no
get_next_free_slot (void)
{
  lock_acquire (&free_slot_list_lock);
  struct list_elem *free_range_elem = list_begin (&free_slot_list);
  if (free_range_elem == list_tail (&free_slot_list))
    {
      /* Out of swap file space! Panic! */
      PANIC ("Out of swap space!");
    }

  struct range *free_range = range_entry (free_range_elem);
  int ret = free_range->start;
  free_range->start++;

  /* Remove the range if it's empty. */
  if (free_range->start == free_range->end) {
    delete_range(free_range);
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
  int new = range_entry (new_elem)->start;
  int ori = range_entry (ori_elem)->start;

  return new < ori;
}