#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include <hash.h>
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"

/* Max size of stack in bytes */
#define STACK_MAX (1024 * 1024)

struct spt_entry *spt_entry_create (void *vaddr, bool read_only)
{
    ASSERT (NULL != vaddr);

    struct spt_entry *p = malloc (sizeof(struct spt_entry));
    if (NULL == p)
        return NULL;

    p->vaddr        = pg_round_down (vaddr);
    p->read_only    = read_only;
    p->private      = !read_only;
    p->frame        = NULL;
    p->swap_index   = BLOCK_SECTOR_NONE;
    p->file         = NULL;
    p->thread       = thread_current ();
    p->file_offset  = 0;
    p->file_bytes   = 0;
    p->is_loaded    = false;
    p->zero_bytes   = 0;

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
            spt_page_out (p);
        frame_release (f);
    }

    hash_delete (thread_current ()->pages, &p->hash_elem);
    free (p);
}

static void page_destroy (struct hash_elem *p_elem, void *aux UNUSED)
{
    struct spt_entry *p = hash_entry (p_elem, struct spt_entry, hash_elem);
    frame_lock (p);
    if (p->frame)
        frame_release (p->frame);
    free (p);
    p = NULL;
}

void spt_destroy (void)
{
    struct hash *h = thread_current ()->pages;
    if (h)
        hash_destroy (h, page_destroy);
}

struct spt_entry *spt_entry_lookup (const void *faddr)
{
    ASSERT (NULL != faddr);
    
    if (PHYS_BASE > faddr)
    {
        struct spt_entry p;
        struct hash_elem *e;

        p.vaddr = (void *) pg_round_down (faddr);
        e = hash_find (thread_current ()->pages, &p.hash_elem);
        if (e)
            return hash_entry (e, struct spt_entry, hash_elem);

        /* Comment block */
        if ((((char *) PHYS_BASE - STACK_MAX) < (char *) faddr) &&
            (((char *) thread_current ()->esp - PUSHA_SZ)) < (char *) faddr)
            return spt_entry_create (p.vaddr, false);
    }

    return NULL;
}

static bool page_in_helper (struct spt_entry * page)
{
    ASSERT (NULL != page);
    page->frame = frame_allocate (page);
    if (NULL == page->frame)
        return false;

    /* copy data over to frame */
    if (BLOCK_SECTOR_NONE != page->swap_index)
        /* get data from a swap */
        swap_in (page);
    else if (page->file)
    {
        /* get data from file using file_read_at (filesys/file.h) */
        off_t read_bytes = file_read_at (
                                            page->file,
                                            page->frame->base_vaddr,
                                            page->file_bytes,
                                            page->file_offset
                                        );
        off_t zero_bytes = PGSIZE - read_bytes;
        memset (((char *) page->frame->base_vaddr + read_bytes), 0, zero_bytes);
        if (page->file_bytes != read_bytes)
            printf ("BYTES READ != BYTES REQUESTED: (%"PROTd") : (%"PROTd")\n", 
                    read_bytes, page->file_bytes);
    }
    else
        /* Provide zeroized page */
        memset (page->frame->base_vaddr, 0, PGSIZE);

    return true;
}

bool spt_page_in (void *faddr)
{
    // ASSERT (NULL != faddr);

    struct spt_entry *p = NULL;
    bool success = false;

    if (NULL == thread_current ()->pages)
        return false;

    p = spt_entry_lookup (faddr);
    if (NULL == p)
        return false;

    frame_lock (p);
    if (NULL == p->frame)
    {
        if (!page_in_helper (p))
            return false;
    }

    ASSERT (lock_held_by_current_thread (&p->frame->frame_lock));

    /* Set frame into page table */
    success = pagedir_set_page(
                                thread_current ()->pagedir,
                                p->vaddr,
                                p->frame->base_vaddr,
                                !p->read_only
                            );

    /* Unlock frame */
    frame_unlock (p->frame);
    return success;
}

bool spt_page_out (struct spt_entry *p)
{
    ASSERT (NULL != p);
    ASSERT (NULL != p->frame);
    ASSERT (lock_held_by_current_thread (&p->frame->frame_lock));

    bool is_dirty = false;
    bool is_swappable = false;

    /* Mark current page not present before dirty check to avoid race condition */
    pagedir_clear_page (p->thread->pagedir, (void *) p->vaddr);

    /* Check for dirty page */
    is_dirty = pagedir_is_dirty (p->thread->pagedir, ((const void *) p->vaddr));

    /* If file is not present, swap */
    if (NULL == p->file)
        is_swappable = swap_out (p);
    /* If file is present, but is dirty */
    else if (is_dirty)
    {
        /* If private, swap, else write to file */
        if (p->private)
            is_swappable = swap_out (p);
        else
            /* write to a file using file_write_at (filesys/file.h) */
            is_swappable = file_write_at (
                                            p->file,
                                            (const void *) p->frame->base_vaddr,
                                            p->file_bytes,
                                            p->file_offset
                                        );
    }
    /* if file is not dirty, no swap needed */
    else if (!is_dirty)
        is_swappable = true;

    /* swap was successful */
    if (is_swappable)
        p->frame = NULL;

    return is_swappable;
}

bool spt_recent (struct spt_entry *p)
{
    ASSERT (NULL != p);
    ASSERT (NULL != p->frame);
    ASSERT (lock_held_by_current_thread (&p->frame->frame_lock));

    bool was_accessed = pagedir_is_accessed (p->thread->pagedir, p->vaddr);
    if (was_accessed)
        pagedir_set_accessed(p->thread->pagedir, p->vaddr, false);
    return was_accessed;
}

bool spt_lock (const void *vaddr, bool writable)
{
    ASSERT (NULL != vaddr);

    struct spt_entry *p = spt_entry_lookup (vaddr);
    if ((NULL == p) || ((p->read_only) && (writable)))
        return false;

    frame_lock (p);
    if (NULL == p->frame)
        return ((page_in_helper (p)) && (pagedir_set_page (thread_current ()->pagedir,
                                                                p->vaddr,
                                                                p->frame->base_vaddr,
                                                                !p->read_only)));
    else
     return true;
}

void spt_unlock (const void *vaddr)
{
    ASSERT (NULL != vaddr);
    struct spt_entry *p = spt_entry_lookup (vaddr);
    ASSERT (NULL != p);
    frame_unlock (p->frame);
}

unsigned spt_entry_hash (const struct hash_elem *e, void *aux UNUSED)
{
    const struct spt_entry *p = hash_entry (e, struct spt_entry, hash_elem);
    return ((uintptr_t) p->vaddr) >> PGBITS;
}

bool spt_less_func (const struct hash_elem *a, const struct hash_elem *b)
{
    return (hash_entry (a, struct spt_entry, hash_elem)->vaddr < hash_entry (b, struct spt_entry, hash_elem)->vaddr);
}