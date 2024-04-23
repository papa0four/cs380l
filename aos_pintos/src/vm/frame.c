<<<<<<< HEAD
#include "vm/frame.h"
#include <stdio.h>
#include <bitmap.h>
#include <round.h>
#include "vm/page.h"
#include "vm/swap.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"


static struct bitmap *frames_available;
static struct frame_entry *frame_table;
static unsigned frame_idx;
static unsigned max_idx;

static struct lock frame_lock;

void
frame_init (size_t user_pages)
{
  unsigned i;

  /* Same thing for calculating pages in init_pool from palloc.c */
  size_t bm_pages = DIV_ROUND_UP (bitmap_buf_size (user_pages), PGSIZE);
  if (bm_pages > user_pages)
    bm_pages = user_pages;
  user_pages -= bm_pages;

  frame_idx = 0;
  max_idx = (unsigned) user_pages;

  /* The frame table contains one entry for each frame that contains a user page */
  frame_table = (struct frame_entry*)malloc(sizeof(struct frame_entry)*user_pages);
  frames_available = bitmap_create(user_pages);

  for(i = 0; i < user_pages; i++) {
    frame_table[i].frame_id = i;
    frame_table[i].page = NULL;
  }

  lock_init (&frame_lock);
}

struct frame_entry *
frame_get_multiple (size_t page_cnt)
{
  size_t fnum = bitmap_scan_and_flip (frames_available, 0, page_cnt, false);
  if (BITMAP_ERROR != fnum)
  {
    frame_table[fnum].kpage = palloc_get_page(PAL_USER | PAL_ASSERT | PAL_ZERO);
    return &frame_table[fnum];
  }
  else {
    /* Evict frame */
    struct thread *t = thread_current ();
    while (pagedir_is_accessed (t->pagedir, frame_table[frame_idx].page->addr))
    {
      pagedir_set_accessed (t->pagedir, frame_table[frame_idx].page->addr, false);
      frame_idx++;
      if(frame_idx >= max_idx)
        frame_idx = 0;
    }
    /* Swap frames */
    swap_in (frame_table[frame_idx].page);
    frame_table[frame_idx].page->frame = NULL;
    frame_table[frame_idx].page->status = SWAP_MAPPED;
    pagedir_clear_page (frame_table[frame_idx].page->pagedir, frame_table[frame_idx].page->addr);
    
    unsigned prev_idx = frame_idx;
    frame_idx++;
    if(frame_idx >= max_idx)
        frame_idx = 0;

    return &frame_table[prev_idx];
  }
}

struct frame_entry *
frame_get (void)
{
  return frame_get_multiple (1);
}

/* Free given frame occupied by terminating process */
void
frame_free (struct frame_entry *f)
{
  pagedir_clear_page (f->page->pagedir, f->page->addr);
  bitmap_reset (frames_available, f->frame_id);
  palloc_free_page (frame_table[f->frame_id].kpage);
  f->page = NULL;
}
=======
>>>>>>> f9b93c36c52a54dfbfcce5d528f35e7454d9c996
