#include "vm/swap.h"
#include <stdio.h>
#include <stdint.h>
#include <bitmap.h>
#include "devices/block.h"
#include "vm/frame.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"

static struct bitmap *used_blocks;
struct block *swap_table;
static struct lock block_lock;

void
swap_init (void)
{
  used_blocks = bitmap_create(BLOCK_BITMAP_SIZE);
  swap_table = block_get_role(BLOCK_SWAP);
  lock_init (&block_lock);
}

/* Block sector size is 512 bytes => 8 sectors = 1 page */

/* Insert the contents of a page into the swap table */
void
swap_in (struct spt_entry *p)
{
  lock_acquire (&block_lock);
  char *byte = (char *) p->frame->kpage;
  size_t sector_num = bitmap_scan_and_flip (used_blocks, 0, 1, false);
  p->sector = sector_num;

  
  for (int i = 0; i < MAX_SECTORS; i++)
  {
    block_write (swap_table, sector_num * MAX_SECTORS + i, byte);
    byte += BLOCK_SECTOR_SIZE;
  }
  lock_release (&block_lock);
}

/* Read from swap into the page */
void
swap_out (struct spt_entry *p)
{
  lock_acquire (&block_lock);
  char *byte = (char *) p->frame->kpage;
  unsigned read_sector = p->sector;

  for (int i = 0; i < MAX_SECTORS; i++)
  {
    block_read (swap_table, read_sector * MAX_SECTORS + i, byte);
    byte += BLOCK_SECTOR_SIZE;
  }

  bitmap_reset (used_blocks, read_sector);
  lock_release (&block_lock);  
}

void
swap_free (struct spt_entry *p)
{
  lock_acquire (&block_lock);
  bitmap_reset (used_blocks, p->sector);
  lock_release (&block_lock);
}