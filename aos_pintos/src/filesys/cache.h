#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include <stdbool.h>
#include "devices/block.h"
#include "devices/timer.h"
#include "threads/synch.h"

#define CACHE_MAX 64

struct disk_cache
{
    uint8_t block[BLOCK_SECTOR_SIZE];
    block_sector_t disk_sector;
    bool is_free;
    int open_cnt;
    bool accessed;
    bool dirty;
};

struct lock cache_lock;
struct disk_cache cache[CACHE_MAX];

void init_entry (int index);
void cache_init (void);
int cache_get_entry (block_sector_t disk_sector);
int cache_get_available (void);
int cache_access_entry (block_sector_t disk_sector, bool dirty);
int cache_replace_entry (block_sector_t disk_sector, bool dirty);
void periodic_write_func (void *aux);
void read_ahead_func (void *aux);
void write_back (bool clear);
void read_ahead (block_sector_t disk_sector);

#endif /* filesys/cache.h */