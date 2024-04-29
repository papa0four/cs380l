#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/block.h"

struct bitmap;

void inode_init (void);
bool inode_create (block_sector_t, off_t, bool, bool);
struct inode *inode_open (block_sector_t);
struct inode *inode_reopen (struct inode *);
block_sector_t inode_get_inumber (const struct inode *);
void inode_close (struct inode *);
void inode_remove (struct inode *);
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write (struct inode *);
void inode_allow_write (struct inode *);
off_t inode_length (const struct inode *);
bool inode_get_symlink (struct inode *inode);
void inode_set_symlink (struct inode *inode, bool is_symlink);

bool inode_is_dir (const struct inode *);
int inode_get_open_cnt (const struct inode *);
// size_t inode_get_used_cnt (const struct inode *);
block_sector_t inode_get_parent (const struct inode *);
bool inode_set_parent (block_sector_t parent, block_sector_t child);

void set_total_sectors (size_t sectors);
bool is_valid_sector (block_sector_t sector);
size_t inode_get_physical_size (const struct inode *inode);
size_t inode_get_block_cnt (const struct inode *inode);
void inode_lock (const struct inode *inode);
void inode_unlock (const struct inode *inode);

#endif /* filesys/inode.h */
