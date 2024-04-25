#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "threads/synch.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include <stdlib.h>

#define CACHE_MAX_SIZE 64

struct disk_cache {
    
    uint8_t block[BLOCK_SECTOR_SIZE]; //size 512B
    block_sector_t disk_sector;

    bool is_free;
    int open_cnt;
    bool accessed;
    bool dirty;
};

struct lock cache_lock;
struct disk_cache cache_array[64];

void init_entry(int idx);
void cache_init (void);
int cache_get_entry (block_sector_t disk_sector);
int cache_get_free (void);
int cache_access_entry (block_sector_t disk_sector, bool dirty);
int cache_replace_entry (block_sector_t disk_sector, bool dirty);
void periodic_write_func (void *aux);
void write_back (bool clear);
void read_ahead_func (void *aux);
void read_ahead (block_sector_t);

#endif /* filesys/cache.h */