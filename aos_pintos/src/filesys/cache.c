#include "filesys/cache.h"
#include <debug.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "cache.h"
#include "devices/block.h"
#include "devices/timer.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"

void init_entry(int idx)
{
  cache_array[idx].is_free = true;
  cache_array[idx].open_cnt = 0;
  cache_array[idx].dirty = false;
  cache_array[idx].accessed = false;
}

void cache_init (void)
{
  lock_init (&cache_lock);
  for (int i = 0; i < CACHE_MAX_SIZE; i++)
  {
    cache_array[i].block[i] = 0;
    init_entry (i);
  }

  thread_create ("cache_writeback", 0, periodic_write_func, NULL);
}

int cache_get_entry (block_sector_t disk_sector)
{
  for(int i = 0; i < CACHE_MAX_SIZE; i++)
  {
    if (disk_sector == cache_array[i].disk_sector)
    {
      if (!cache_array[i].is_free)
        return i;
    }
  }
    
  return -1;
}

int cache_get_free (void)
{
  for (int i = 0; i < CACHE_MAX_SIZE; i++)
  {
    if (cache_array[i].is_free == true)
    {
      cache_array[i].is_free = false;
      return i;
    }
  }

  return -1;
}

int cache_access_entry (block_sector_t disk_sector, bool dirty)
{
  lock_acquire (&cache_lock);
  
  int idx = cache_get_entry (disk_sector);
  if(-1 == idx)
    idx = cache_replace_entry (disk_sector, dirty);
  else
  {
    cache_array[idx].open_cnt++;
    cache_array[idx].accessed = true;
    cache_array[idx].dirty |= dirty;
  }

  lock_release (&cache_lock);
  return idx;
}

int cache_replace_entry (block_sector_t disk_sector, bool dirty)
{
  // lock_acquire (&cache_lock);
  int idx = cache_get_free ();
  if (-1 == idx) //cache is full
  {
    for (int i = 0; ; i = (i + 1) % CACHE_MAX_SIZE)
    {
      /* Cache entry is in use */
      if (cache_array[i].open_cnt > 0)
        continue;

      /* Determine if the entry has been accessed */
      if (cache_array[i].accessed)
        cache_array[i].accessed = false;
      else
      {
        /* If not, evict it if dirty, write to disk */
        if (cache_array[i].dirty)
          block_write (fs_device, cache_array[i].disk_sector, &cache_array[i].block);

        init_entry (i);
        idx = i;
        break;
      }
    }
  }

  cache_array[idx].disk_sector = disk_sector;
  cache_array[idx].is_free = false;
  cache_array[idx].open_cnt++;
  cache_array[idx].accessed = true;
  cache_array[idx].dirty = dirty;
  block_read (fs_device, cache_array[idx].disk_sector, &cache_array[idx].block);

  // lock_release (&cache_lock);
  return idx;
}

void periodic_write_func (void *aux UNUSED)
{
  while (true)
  {
    timer_sleep (TIMER_FREQ);
    write_back (false);
  }
}

void write_back (bool clear)
{
  lock_acquire (&cache_lock);
  for (int i = 0; i < CACHE_MAX_SIZE; i++)
  {
    if (cache_array[i].dirty)
    {
      block_write (fs_device, cache_array[i].disk_sector, &cache_array[i].block);
      cache_array[i].dirty = false;
    }

    // clear cache line (filesys done)
    if(clear)
      init_entry (i);
  }
  lock_release (&cache_lock);
}

// void read_ahead_func (void *aux)
// {
//   block_sector_t disk_sector = *(block_sector_t *)aux;
//   lock_acquire (&cache_lock);

//   int idx = cache_get_entry (disk_sector);

//   // need eviction
//   if (-1 == idx)
//     cache_replace_entry (disk_sector, false);

//   lock_release (&cache_lock);
//   free (aux);
// }

// void read_ahead (block_sector_t disk_sector)
// {
//     block_sector_t *arg = malloc (sizeof (block_sector_t));
//     *arg = disk_sector + 1;  // next block
//     thread_create ("cache_read_ahead", 0, read_ahead_func, arg);
// }