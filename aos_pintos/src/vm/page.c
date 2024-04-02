#include "vm/page.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

struct spt_entry *spt_entry_create (void *vaddr, bool writable)
{
    ASSERT (NULL != vaddr);

    struct spt_entry *p = malloc (sizeof(struct spt_entry));
    if (NULL == p)
        return NULL;

    p->vaddr        = pg_round_down (vaddr);
    p->writable     = writable;
    p->private      = !writable;
    p->frame        = NULL;
    p->sector       = BLOCK_SECTOR_NONE;
    p->file         = NULL;
    p->thread       = thread_current ();
    p->file_offset  = 0;
    p->file_bytes   = 0;
    p->is_loaded    = false;
    p->read_bytes   = 0;
    p->zero_bytes   = 0;
    p->swap_index   = 0;

    if (NULL != hash_insert (thread_current ()->pages, &p->hash_elem))
    {
        /* page is already mapped */
        free (p);
        p = NULL;
    }

    return p;
}