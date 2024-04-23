#include "filesys/cache.h"
#include <stdbool.h>
#include "cache.h"
#include "devices/timer.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"

void init_entry (int index)
{
    cache[index].is_free    = true;
    cache[index].open_cnt   = 0;
    cache[index].dirty      = false;
    cache[index].accessed   = false;
}

void cache_init (void)
{
    lock_init (cache_lock);
    for (int i = 0; i < CACHE_MAX; i++)
        init_entry (i);

    thread_create ("cache_write_back", 0, periodic_write_func, NULL);
}

int cache_get_entry(block_sector_t disk_sector)
{
    for (int i = 0; i < CACHE_MAX; i++)
    {
        if (disk_sector == cache[i].disk_sector)
        {
            if (!cache[i].is_free)
                return i;
        }
    }

    return -1;
}

int cahce_get_available (void)
{
    for (int i = 0; i < CACHE_MAX; i++)
    {
        if (cache[i].is_free)
        {
            cache[i].is_free = false;
            return i;
        }
    }

    return -1;
}

int cache_access_entry (block_sector_t disk_sector, bool dirty)
{
    lock_acquire (&cache_lock);

    int index = cache_get_entry (disk_sector);
    if (-1 == index)
        index = cache_replace_entry (disk_sector, dirty);
    else
    {
        cache[index].open_cnt++;
        cache[index].accessed    = true;
        cache[index].dirty      |= dirty;
    }

    lock_release (&cache_lock);
    return index;
}

int cache_replace_entry (block_sector_t disk_sector, bool dirty)
{
    int index = cache_get_available ();
    if (-1 == index)
    {
        for (int i = 0; ; (i + 1) % CACHE_MAX)
        {
            if (0 < cache[i].open_cnt)
                continue;

            if (cache[i].accessed)
                cache[i].accessed = false;
            else
            {
                if (cache[i].dirty)
                    block_write (fs_device, cache[i].disk_sector, &cache[i].block);

                init_entry (i);
                index = i;
                break;
            }
        }
    }

    cache[index].disk_sector    = disk_sector;
    cache[index].is_free        = false;
    cache[index].open_cnt++;
    cache[index].accessed       = true;
    cache[index].dirty          = dirty;

    block_read (fs_device, cache[index].disk_sector, &cache[index].block);
    return index;
}

void periodic_write_func (void *aux UNUSED)
{
    while (1)
    {
        timer_sleep (4 * TIMER_FREQ);
        write_back (false);
    }
}

void write_back (bool clear)
{
    lock_acquire (&cache_lock);
    for (int i = 0; i < CACHE_MAX; i++)
    {
        if (cache[i].dirty)
        {
            block_write (fs_device, cache[i].disk_sector, &cache[i].block);
            cache[i].dirty = false;
        }

        if (clear)
            init_entry (i)
    }

    lock_release (&cache_lock);
}

void read_ahead_func (void *aux)
{
    block_sector_t disk_sector = *(block_sector_t *) aux;
    lock_acquire (&cache_lock);
    int index = cache_get_entry (disk_sector);
    if (-1 == index)
        cache_replace_entry (disk_sector, false);
    lock_release (&cache_lock);
    free (aux);
}

void read_ahead (block_sector_t disk_sector)
{
    block_sector_t *arg = malloc (sizeof (block_sector_t));
    if (NULL == arg)
        return;

    *arg = disk_sector + 1;
    thread_create ("cache_read_ahead", 0, read_ahead_func, arg);
}