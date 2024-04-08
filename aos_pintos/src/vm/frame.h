#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <stdbool.h>
#include "threads/synch.h"

#define MAX_RETRIES 3
#define TIMEOUT_MS 100

/* Add comment here about frame struct */
struct frame
{
    struct spt_entry *page;
    struct lock frame_lock;
    void *base_vaddr;
    unsigned last_accessed;     /* counter of last access for eviction */
};

struct frame_table
{
    struct frame *frames;           /* Dynamic array of frames. */
    struct lock table_lock;         /* Lock to synchronize access to the frame table. */
    struct condition frame_cond;    /* Frame availability condition */
    size_t frame_cnt;               /* Number of frames in the array. */
}

void frame_table_init (void);
struct frame *frame_allocate (struct spt_entry *page);
void frame_lock (struct spt_entry *page);
void frame_release (struct frame *frame);
void frame_unlock (struct frame *frame);

#endif /* vm.frame.h */