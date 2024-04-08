#include "vm/frame.h"
#include "frame.h"
#include "vm/page.h"
#include <stdio.h>
#include <limits.h>
#include "devices/timer.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

static struct frame_table ft;

void frame_table_init (void)
{
    void *vaddr_base = NULL;

    /* Initialize the frame table's members */
    lock_init (&ft.table_lock);
    cond_init (&ft.frame_cond);
    /* allocate the frames array using init_ram_pages (threads/loader.h) */
    ft.frames = malloc (sizeof (struct frame) * init_ram_pages);
    ft.frame_cnt = 0;
    ft.hand = 0;

    if (NULL == ft.frames)
        return;

    while ((NULL != (vaddr_base = palloc_get_page (PAL_USER))) && (init_ram_pages > ft.frame_cnt))
    {
        struct frame *f = &ft.frames[ft.frame_cnt++];
        lock_init (&f->frame_lock);
        f->base_vaddr = vaddr_base;
        f->page = NULL;
    }
}

static struct frame *eviction_check (struct frame *f)
{
    if (f)
    {
        /* Ensure frame is not locked or in use */
        if (!lock_try_acquire (&f->frame_lock))
        {
            /* Current frame is in use and cannot be evicted */
            lock_release (&ft.table_lock);
            return NULL;
        }

        /* Determine if page can be evicted, i.e. dirty page */
        if (f->page)
        {
            void *vaddr = f->page->vaddr;
            if (pagedir_is_dirty (thread_current ()->pagedir, vaddr))
            {
                if (!spt_page_out (f->page))
                {
                    /* Failed to write page to disk or swap */
                    lock_release (&f->frame_lock);
                    lock_release (&ft.table_lock);
                    return NULL;
                }
            }
        }

        /* Page has been successfull evicted, or no page existed. */
        f->page = NULL;
        lock_release (&ft.table_lock);

        /* Return available frame with frame_lock still acquired for safe use. */
        return f;
    }

    lock_release (&ft.table_lock);
    return NULL;
}

static struct frame *frame_find_available (void)
{
    lock_acquire (&ft.table_lock);

    struct frame *evict_candidate = NULL;
    /* Set the starting point greater than or equal to any actual last_access value. */
    unsigned max_access_time = UINT_MAX;

    for (size_t i = 0; i < ft.frame_cnt; i++)
    {
        struct frame *f = &ft.frames[i];
        if (NULL == f->page)
        {
            lock_release (&ft.table_lock);
            return f; // frame found
        }

        /* Look for least recently used page */
        if (max_access_time > f->last_accessed)
        {
            evict_candidate = f;
            max_access_time = f->last_accessed;
        }
    }

    evict_candidate = eviction_check (evict_candidate);
    return evict_candidate; // if found returns free'd frame, else NULL    
}

static struct frame *try_frame_alloc (struct spt_entry *page)
{
    struct frame *f = frame_find_available();
    if (NULL == f)
        return NULL;

    f->page = page;
    return f;
}

static bool is_frame_available (void)
{
    // lock_acquire (&ft.table_lock);
    for (size_t i = 0; i < ft.frame_cnt; i++)
    {
        if (NULL == ft.frames[i].page)
        {
            // lock_release (&ft.table_lock);
            /* free frame found */
            return true;
        }
    }
    // lock_release (&ft.table_lock);
    /* No free frames found */
    return false;
}

static bool wait_for_frame (struct condition frame_cond, int64_t timeout_ms)
{
    struct lock wait_lock;
    lock_init (&wait_lock);
    lock_acquire (&wait_lock);

    /* milliseconds to ticks conversion */
    int64_t start = timer_ticks ();
    int64_t end = start + timer_msleep (timeout_ms);

    while ((!is_frame_available ()) && (timer_elapsed (start) < timeout_ms))
        cond_wait (frame_cond, &wait_lock);

    bool succeeded = is_frame_available ();
    lock_release (&wait_lock);
    return succeeded;
}

struct frame *frame_allocate (struct spt_entry *page)
{
    ASSERT (NULL != page);
    struct frame *f = try_frame_alloc (page);
    if (f)
    {
        ASSERT (lock_held_by_current_thread (&f->frame_lock));
        return f;
    }

    /* Wait for a notification that a frame has become available */
    for (size_t try = 0; try < MAX_RETRIES; try++)
    {
        if (wait_for_frame (&ft.frame_cond, TIMEOUT_MS))
        {
            f = try_frame_alloc (page);
            if (f)
            {
                ASSERT (lock_held_by_current_thread (&f->frame_lock));
                return f;
            }
            else
                /* Break early if timeout reached with no available frame. */
                break;
        }
    }

    /* return null if no frame is created */
    return NULL;
}

void frame_lock (struct spt_entry *page)
{
    ASSERT (NULL != page);
    struct frame *f = page->frame;
    if (f)
    {
        lock_acquire (&f->frame_lock);
        if (page->frame != f)
        {
            lock_release (&f->frame_lock);
            ASSERT (NULL == page->frame);
        }
    }
}

void frame_release (struct frame *frame)
{
    ASSERT (lock_held_by_current_thread (&frame->frame_lock));
    frame->page = NULL;
    cond_signal (&ft.frame_cond, &ft.table_lock);
    lock_release (&frame->frame_lock);
}

void frame_unlock (struct frame *frame)
{
    ASSERT (lock_held_by_current_thread (&frame->frame_lock));
    lock_release (&frame->frame_lock);
}