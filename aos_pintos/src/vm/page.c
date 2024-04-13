#include "vm/page.h"
#include <stdio.h>
#include <stdint.h>
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "vm/swap.h"


/* hash function, address comparator */
/* Returns a hash value for page p. */
unsigned
spt_hash_func (const struct hash_elem *p_, void *aux UNUSED)
{
  const struct spt_entry *p = hash_entry (p_, struct spt_entry, hash_elem);
  return hash_bytes (&p->addr, sizeof p->addr);
}

/* Returns true if page a precedes page b. */
bool
spt_less_func (const struct hash_elem *a_, const struct hash_elem *b_,
           void *aux UNUSED)
{
  const struct spt_entry *a = hash_entry (a_, struct spt_entry, hash_elem);
  const struct spt_entry *b = hash_entry (b_, struct spt_entry, hash_elem);

  return pg_no(a->addr) < pg_no(b->addr);
}

void
page_destructor (struct hash_elem *e, void *aux UNUSED)
{
  struct spt_entry *p = hash_entry (e, struct spt_entry, hash_elem);
  ASSERT (NULL != p);
  if (p->frame != NULL) {
    struct frame_entry *f = p->frame;
    p->frame = NULL;
    frame_free (f);
  }
  if (p->sector != -1)
    swap_free (p);
  free (p);
}

/* Function to search the hash table */
/* Returns the page containing the given virtual address, */
/* or a null pointer if no such page exists. */
struct spt_entry *
page_lookup (void *address)
{
  struct thread *t = thread_current ();
  struct spt_entry p;
  struct hash_elem *e;

  p.addr = (void *) (pg_no(address) << PGBITS);
  e = hash_find (&t->pages, &p.hash_elem);
  return e != NULL ? hash_entry (e, struct spt_entry, hash_elem) : NULL;
}