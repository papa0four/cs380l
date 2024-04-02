#include "vm/page.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"

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

void spt_entry_remove (void *vaddr)
{
    struct spt_entry *p = spt_entry_lookup (vaddr);
    ASSERT (NULL != p);

    /* Place holder for lock */
    if (p->frame)
    {
        struct frame *f = p->frame;
        if ((p->file) && (!p->private))
            spt_entry_out (p);
        frame_free (f);
    }

    hash_delete (thread_current ()->pages, &p->hash_elem);
    free (p);
}

struct spt_entry *spt_entry_lookup (const void *vaddr)
{
    ASSERT (NULL != vaddr);
    
    if (PHYS_BASE > vaddr)
    {
        struct spt_entry p;
        struct hash_elem *e;

        p.vaddr = (void *) pg_round_down (vaddr);
        e = hash_find (thread_current ()->pages, &p.hash_elem);
        if (e)
            return hash_entry (e, struct spt_entry, hash_elem);

        /* Comment block */
        if (((PHYS_BASE - MAX_STACK_SZ) < p.vaddr) && (((void *) thread_current ()->esp - PUSHA_SZ)) < vaddr)
            return spt_entry_create (p.addr, false);
    }

    return NULL;
}