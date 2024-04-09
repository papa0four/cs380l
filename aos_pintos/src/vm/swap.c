#include "vm/swap.h"
#include <bitmap.h>
#include <debug.h>
#include <stdio.h>
#include "vm/frame.h"
#include "vm/page.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

static struct block *swap_device;

static struct bitmap *swap_bitmap;

static struct lock swap_lock;

#define NUM_SECTORS (PGSIZE / BLOCK_SECTOR_SIZE)

void swap_init (void)
{
    swap_device = block_get_role (BLOCK_SWAP);

    if (NULL == swap_device)
        swap_bitmap = bitmap_create (0);
    else
        swap_bitmap = bitmap_create ((block_size (swap_device) / NUM_SECTORS));

    if (NULL == swap_bitmap)
        return;
    
    lock_init (&swap_lock);
}

void swap_in (struct spt_entry *p)
{
    ASSERT (NULL != p);
    ASSERT (NULL != p->frame);
    ASSERT (BLOCK_SECTOR_NONE != p->swap_index);
    ASSERT (lock_held_by_current_thread (&p->frame->frame_lock));

    for (size_t i = 0; i < NUM_SECTORS; i++)
        block_read (
            swap_device,
            (p->swap_index + i),
            (void *)((char *) p->frame->base_vaddr + i * BLOCK_SECTOR_SIZE)
        );
    
    bitmap_reset (
        swap_bitmap,
        ((size_t) p->swap_index / NUM_SECTORS)
        );

    p->swap_index = BLOCK_SECTOR_NONE;
}

bool swap_out (struct spt_entry *p)
{
    ASSERT (NULL != p);
    ASSERT (NULL != p->frame);
    ASSERT (lock_held_by_current_thread (&p->frame->frame_lock));

    lock_acquire (&swap_lock);
    size_t slot = bitmap_scan_and_flip (swap_bitmap, 0, 1, false);
    lock_release (&swap_lock);

    if (BITMAP_ERROR == slot)
        return false;

    p->swap_index = (slot * NUM_SECTORS);

    for (size_t i = 0; i < NUM_SECTORS; i++)
        block_write (
            swap_device,
            ((size_t) p->swap_index + i),
            (void*)((char *) p->frame->base_vaddr + i * BLOCK_SECTOR_SIZE)
        );

    p->private      = false;
    p->file         = NULL;
    p->file_offset  = 0;
    p->file_bytes   = 0;

    return true;
}