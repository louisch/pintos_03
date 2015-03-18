#include <list.h>
#include "swap.h"

/* Struct inserted into the list of free ranges,
   marks start (inclusive) and end of range (exclusive). */
struct range_elem
  {
    slot_no start; /* Number of first free swap slot. */
    slot_no end; /* End of range (exclusive i.e. a used slot). */
    struct list_elem elem;
  };

struct list free_slots;

static slot_no get_next_free_page (void);
static void free_page_slot (slot_no);

/* Initialise swap table. */
void
swap_init (void)
  {
    list_init (&free_slots);
  }

/* Insert a page into the swap table. */
void
swap_insert (void *kpage)
  {
    get_next_free_page ();
  }

/* Frees all the swap slots occupied by the array of pages passed in. */
void
free_used_frames (void) // ARG HAXX
  {
    return NULL; // RETURN HAXX
  }

static slot_no
get_next_free_page (void)
  {
    return NULL; // MOAR RETURN HAXX
  }

/* Frees a page slot belonging to a file and combines ranges when necessary. */
static void
free_page_slot (slot_no)
  {
    return NULL; // RETURN HAXX
  }