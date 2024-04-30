#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "devices/block.h"
#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "filesys/off_t.h"
#include "threads/malloc.h"
#include "threads/synch.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* MACROS for on-disk inode enhancements */
#define DIRECT_BLOCKS         4
#define INDIRECT_BLOCKS       9
#define DBL_INDIRECT_BLOCKS   1
#define INODE_BLOCKS          14

#define BLOCK_PTR_SIZE sizeof (block_sector_t)
#define SECTORS_PER_IBLOCK (BLOCK_SECTOR_SIZE / sizeof (block_sector_t))

/* Math behind padding
  off_t length = 4 bytes
  unsigned magic = 4 bytes
  block_sector_t blocks[INODE_BLOCKS] = 14 * 4 = 56
  uint32_t direct, indirect, d_indirect = 3 * 4 = 12
  bool is_dir & is_symlink = 2 * 4 = 8
  block_sector_t parent = 4
  4 + 4 + 56 + 12 + 8 + 4 = 88
  512 - 88 = 424
  424 / 4 = 106 + 1 to account for start member removal
*/
#define PADDING 107

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long.
*/
struct inode_disk
{
  off_t length;                     /* File size in bytes */
  unsigned magic;                   /* Magic INODE number */
  uint32_t unused[PADDING];         /* Padding to maintain size */

  /* User Addition */
  uint32_t direct_idx;                  /* Current index of direct blocks */
  uint32_t indirect_idx;                /* Current index of indirect blocks*/
  uint32_t d_indirect_idx;              /* Current index of dbl indirect */
  block_sector_t blocks[INODE_BLOCKS];  /* Array of block indices */
  bool is_dir;                          /* Is directory flag */
  bool is_symlink;                      /* Is symbolic link flag */
  block_sector_t parent;                /* Parent directory sector number */
};

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long.
*/
static inline size_t bytes_to_sectors (off_t size) { return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE); }

/* In-memory inode. */
struct inode 
{
  struct list_elem elem;              /* Element in inode list. */
  block_sector_t sector;              /* Sector number of disk location. */
  int open_cnt;                       /* Number of openers. */
  bool removed;                       /* True if deleted, false otherwise. */
  int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */

  /* User Addition */
  off_t length;                         /* File size in bytes */
  off_t read_length;                    /* Readable length of file */
  uint32_t direct_idx;                  /* Current index of direct blocks */
  uint32_t indirect_idx;                /* Current index of indirect blocks */
  uint32_t d_indirect_idx;              /* Current index of dbl indirect */
  block_sector_t blocks[INODE_BLOCKS];  /* Array of block indices */
  bool is_dir;                          /* Is directory flag */
  bool is_symlink;                      /* Is symbolic link flag */
  block_sector_t parent;                /* Parent directory sector number */
  struct lock lock;                     /* Synchronization lock */
};

/*User Implemented Helper function declarations */
static bool inode_allocate (struct inode_disk *disk_inode);
static off_t inode_extend (struct inode* inode, off_t length);
static void inode_release (struct inode *inode);
static void handle_direct (struct inode **inode, size_t *sectors);
static void handle_indirect (struct inode **inode, size_t *sectors);
static void handle_dbl_indirect (struct inode **inode, size_t *sectors);
static void free_direct (struct inode **inode, size_t *sector, size_t *index);
static void free_indirect (struct inode *inode, size_t *sector, size_t *index);
static void free_dbl_indirect (struct inode *inode, size_t *sector);


/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t byte_to_sector (const struct inode *inode, off_t length, off_t pos) 
{
  ASSERT (NULL != inode);
  if (pos < length)
  {
    uint32_t idx = 0;
    uint32_t blocks[SECTORS_PER_IBLOCK];

    /* Handle direct blocks */
    if (pos < (DIRECT_BLOCKS * BLOCK_SECTOR_SIZE))
      return inode->blocks[pos / BLOCK_SECTOR_SIZE];

    /* Handle indirect blocks */
    else if ((off_t)((DIRECT_BLOCKS + INDIRECT_BLOCKS * SECTORS_PER_IBLOCK)
      * BLOCK_SECTOR_SIZE) > pos)
    {
      /* Read corresponding indirect block */
      pos -= DIRECT_BLOCKS * BLOCK_SECTOR_SIZE;
      idx = pos / (SECTORS_PER_IBLOCK * BLOCK_SECTOR_SIZE) + DIRECT_BLOCKS;
      block_read (fs_device, inode->blocks[idx], &blocks);

      pos %= SECTORS_PER_IBLOCK * BLOCK_SECTOR_SIZE;
      return blocks[pos / BLOCK_SECTOR_SIZE];
    }

    /* Handle double indirect blocks */
    else
    {
      /* Read first level block */
      block_read (fs_device, inode->blocks[INODE_BLOCKS - 1], &blocks);

      /* Read second level block */
      pos -= (DIRECT_BLOCKS + INDIRECT_BLOCKS * SECTORS_PER_IBLOCK) * BLOCK_SECTOR_SIZE;
      idx = pos / (SECTORS_PER_IBLOCK * BLOCK_SECTOR_SIZE);
      block_read (fs_device, blocks[idx], &blocks);

      pos %= SECTORS_PER_IBLOCK * BLOCK_SECTOR_SIZE;
      return blocks[pos / BLOCK_SECTOR_SIZE];
    }
  }
  
  return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'.
*/
static struct list open_inodes;

static size_t total_device_sectors = 0;

/* Initializes the inode module. */
void inode_init (void) { list_init (&open_inodes); }

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool inode_create (block_sector_t sector, off_t length, bool is_dir, bool is_symlink)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (0 <= length);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that.
  */
  ASSERT (BLOCK_SECTOR_SIZE == sizeof (*disk_inode));

  disk_inode = calloc (1, sizeof (*disk_inode));
  if (disk_inode)
  {
    /* Set disk inode members */
    disk_inode->length      = length;
    disk_inode->magic       = INODE_MAGIC;
    disk_inode->is_dir      = is_dir;
    disk_inode->is_symlink  = is_symlink;
    disk_inode->parent      = ROOT_DIR_SECTOR;
    if (inode_allocate (disk_inode)) 
    {
      block_write (fs_device, sector, disk_inode);
      success = true; 
    }

    free (disk_inode);
  }
  
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
  {
    inode = list_entry (e, struct inode, elem);
    if (sector == inode->sector) 
      {
        inode_reopen (inode);
        return inode; 
      }
  }

  /* Allocate memory. */
  inode = malloc (sizeof (*inode));
  if (NULL == inode)
    return NULL;

  /* Initialize. */
  struct inode_disk disk_inode;

  list_push_front (&open_inodes, &inode->elem);
  inode->sector         = sector;
  inode->open_cnt       = 1;
  inode->deny_write_cnt = 0;
  inode->removed        = false;
  lock_init (&inode->lock);

  /* Copy disk data to inode */
  block_read (fs_device, inode->sector, &disk_inode);
  inode->length           = disk_inode.length;
  inode->read_length      = disk_inode.length;
  inode->direct_idx       = disk_inode.direct_idx;
  inode->indirect_idx     = disk_inode.indirect_idx;
  inode->d_indirect_idx   = disk_inode.d_indirect_idx;
  inode->is_dir           = disk_inode.is_dir;
  inode->is_symlink       = disk_inode.is_symlink;
  inode->parent           = disk_inode.parent;
  memcpy (&inode->blocks, &disk_inode.blocks, INODE_BLOCKS * BLOCK_PTR_SIZE);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *inode_reopen (struct inode *inode)
{
  if (NULL != inode)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t inode_get_inumber (const struct inode *inode) { return inode->sector; }

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks.
*/
void inode_close (struct inode *inode) 
{
  struct inode_disk disk_inode;

  /* Ignore null pointer. */
  if (NULL == inode)
    return;

  /* Release resources if this was the last opener. */
  if (0 == --inode->open_cnt)
  {
    /* Remove from inode list and release lock. */
    list_remove (&inode->elem);

    /* Deallocate blocks if removed. */
    if (inode->removed) 
    {
      free_map_release (inode->sector, 1);
      inode_release (inode);
    }
    else
    {
      disk_inode.length          = inode->length;
      disk_inode.magic           = INODE_MAGIC;
      disk_inode.direct_idx      = inode->direct_idx;
      disk_inode.indirect_idx    = inode->indirect_idx;
      disk_inode.d_indirect_idx  = inode->d_indirect_idx;
      disk_inode.is_dir          = inode->is_dir;
      disk_inode.is_symlink      = inode->is_symlink;
      disk_inode.parent          = inode->parent;
      memcpy (&disk_inode.blocks, &inode->blocks,
      INODE_BLOCKS * BLOCK_PTR_SIZE);
      block_write (fs_device, inode->sector, &disk_inode);
    }

    free (inode); 
  }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open.
*/
void inode_remove (struct inode *inode) 
{
  ASSERT (NULL != inode);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;

  off_t length = inode->read_length;
  if(offset >= length)
    return 0;

  while (0 < size) 
  {
    /* Disk sector to read, starting byte offset within sector. */
    block_sector_t sector_idx = byte_to_sector (inode, length, offset);
    int sector_ofs = offset % BLOCK_SECTOR_SIZE;

    /* Bytes left in inode, bytes left in sector, lesser of the two. */
    off_t inode_left = length - offset;
    int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    int min_left = inode_left < sector_left ? inode_left : sector_left;

    /* Number of bytes to copy out of sector. */
    int chunk_size = size < min_left ? size : min_left;
    if (0 >= chunk_size)
      break;

    /* Read from cache */
    int cache_idx = cache_access_entry (sector_idx, false);
    memcpy (buffer + bytes_read, cache_array[cache_idx].block + sector_ofs,
      chunk_size);
    cache_array[cache_idx].accessed = true;
    cache_array[cache_idx].open_cnt--;
    
    /* Advance. */
    size        -= chunk_size;
    offset      += chunk_size;
    bytes_read  += chunk_size;
  }

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  

  if (inode->deny_write_cnt)
    return 0;

  /* beyond EOF, need extend */
  if((offset + size) > inode_length(inode))
  {
    /* Synch if file, no sync required for dirs */
    if(!inode->is_dir)
      lock_acquire(&inode->lock);

    inode->length = inode_extend (inode, offset + size);

    if(!inode->is_dir)
      lock_release (&inode->lock);
  }

  while (0 < size) 
  {
    /* Sector to write, starting byte offset within sector. */
    block_sector_t sector_idx = byte_to_sector (inode, inode_length (inode), offset);
    int sector_ofs = offset % BLOCK_SECTOR_SIZE;

    /* Bytes left in inode, bytes left in sector, lesser of the two. */
    off_t inode_left = inode_length (inode) - offset;
    int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    int min_left = inode_left < sector_left ? inode_left : sector_left;

    /* Number of bytes to actually write into this sector. */
    int chunk_size = size < min_left ? size : min_left;
    if (0 >= chunk_size)
      break;

    /* Write to cache */
    int cache_idx = cache_access_entry (sector_idx, true);
    memcpy (cache_array[cache_idx].block + sector_ofs, buffer + bytes_written, chunk_size);
    cache_array[cache_idx].accessed = true;
    cache_array[cache_idx].dirty    = true;
    cache_array[cache_idx].open_cnt--;
    

    /* Advance. */
    size          -= chunk_size;
    offset        += chunk_size;
    bytes_written += chunk_size;
  }


  inode->read_length = inode_length (inode);
  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  // ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode.
*/
void inode_allow_write (struct inode *inode) 
{
  ASSERT (0 < inode->deny_write_cnt);
  // ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t inode_length (const struct inode *inode) { return inode->length; }

bool inode_get_symlink (struct inode *inode)
{ 
  ASSERT (NULL != inode);
  return inode->is_symlink; 
}

void inode_set_symlink (struct inode *inode, bool is_symlink)
{
  inode->is_symlink = is_symlink;
  block_write (fs_device, inode->sector, &inode->blocks);
}

/* User Implemented Helper Functions */
static bool inode_allocate (struct inode_disk *disk_inode)
{
  struct inode inode;
  inode.length          = 0;
  inode.direct_idx      = 0;
  inode.indirect_idx    = 0;
  inode.d_indirect_idx  = 0;

  inode_extend (&inode, disk_inode->length);
  disk_inode->direct_idx     = inode.direct_idx;
  disk_inode->indirect_idx   = inode.indirect_idx;
  disk_inode->d_indirect_idx = inode.d_indirect_idx;
  memcpy(&disk_inode->blocks, &inode.blocks, INODE_BLOCKS * BLOCK_PTR_SIZE);
  return true;
}

static void handle_direct (struct inode **inode, size_t *sectors)
{
  static char zeros[BLOCK_SECTOR_SIZE];
  while ((DIRECT_BLOCKS > (*inode)->direct_idx) && (0 != *sectors))
  {
    free_map_allocate (1, &(*inode)->blocks[(*inode)->direct_idx]);
    block_write (fs_device, (*inode)->blocks[(*inode)->direct_idx], zeros);
    (*inode)->direct_idx++;
    (*sectors)--;
  }
}

static void handle_indirect (struct inode **inode, size_t *sectors)
{
  static char zeros[BLOCK_SECTOR_SIZE];
  while (((DIRECT_BLOCKS + INDIRECT_BLOCKS) > (*inode)->direct_idx) &&
        (0 != *sectors))
  {
    block_sector_t blocks[SECTORS_PER_IBLOCK];

    /* Read first level block */
    if (0 == (*inode)->indirect_idx)
      free_map_allocate (1, &(*inode)->blocks[(*inode)->direct_idx]);
    else
      block_read (fs_device, (*inode)->blocks[(*inode)->direct_idx], &blocks);

    /* Expand block */
    while ((SECTORS_PER_IBLOCK > (*inode)->indirect_idx) && (0 != *sectors))
    {
      free_map_allocate (1, &blocks[(*inode)->indirect_idx]);
      block_write (fs_device, blocks[(*inode)->indirect_idx], zeros);
      (*inode)->indirect_idx++;
      (*sectors)--;
    }
    
    /* Write expanded blocks to disk */
    block_write (fs_device, (*inode)->blocks[(*inode)->direct_idx], &blocks);

    /* Go to new indirect block */
    if (SECTORS_PER_IBLOCK == (*inode)->indirect_idx)
    {
      (*inode)->indirect_idx = 0;
      (*inode)->direct_idx++;
    }
  }
}

static void handle_dbl_indirect (struct inode **inode, size_t *sectors)
{
  static char zeros[BLOCK_SECTOR_SIZE];
  if (((INODE_BLOCKS - 1) == (*inode)->direct_idx) && (0 != *sectors))
  {
    block_sector_t level_one[SECTORS_PER_IBLOCK];
    block_sector_t level_two[SECTORS_PER_IBLOCK];

    /* Read first level block */
    if ((0 == (*inode)->d_indirect_idx) && (0 == (*inode)->indirect_idx))
      free_map_allocate (1, &(*inode)->blocks[(*inode)->direct_idx]);
    else
      block_read (fs_device, (*inode)->blocks[(*inode)->direct_idx], &level_one);

    /* Expand block */
    while ((SECTORS_PER_IBLOCK > (*inode)->indirect_idx) && (0 != *sectors))
    {
      /* Read second level block */
      if (0 == (*inode)->d_indirect_idx)
        free_map_allocate (1, &level_one[(*inode)->indirect_idx]);
      else
        block_read (fs_device, level_one[(*inode)->indirect_idx], &level_two);

      /* Expand block */
      while ((SECTORS_PER_IBLOCK > (*inode)->d_indirect_idx) && (0 != *sectors))
      {
        free_map_allocate (1, &level_two[(*inode)->d_indirect_idx]);
        block_write (fs_device, level_two[(*inode)->d_indirect_idx], zeros);
        (*inode)->d_indirect_idx++;
        (*sectors)--;
      }

      /* Write second level block to disk */
      block_write (fs_device, level_one[(*inode)->indirect_idx], &level_two);

      /* Go to new second level block */
      if (SECTORS_PER_IBLOCK == (*inode)->d_indirect_idx)
      {
        (*inode)->d_indirect_idx = 0;
        (*inode)->indirect_idx++;
      }
    }

    /* Write first level block to disk */
    block_write (fs_device, (*inode)->blocks[(*inode)->direct_idx], &level_one);
  }
}

static off_t inode_extend (struct inode *inode, off_t length)
{
  size_t grow_sectors = bytes_to_sectors(length) - bytes_to_sectors(inode->length);

  if (0 == grow_sectors)
    return length;

  /* Handle direct blocks (index < 4) */
  handle_direct (&inode, &grow_sectors);

  /* Handle indirect blocks (index < 13) */
  handle_indirect (&inode, &grow_sectors);

  /* Handle double indirect blocks (index == 14) */
  handle_dbl_indirect (&inode, &grow_sectors);

  inode->length = length;
  
  return length;
}

static void free_direct (struct inode **inode, size_t *sector, size_t *index)
{
  if ((DIRECT_BLOCKS > *index) && (0 < *sector))
  {
    free_map_release ((*inode)->blocks[*index], 1);
    (*inode)->blocks[*index] = 0;
    (*sector)--;
    (*index)++;
  }
}

static void free_indirect (struct inode *inode, size_t *sector, size_t *index)
{
  while ((DIRECT_BLOCKS <= inode->direct_idx) && 
        ((DIRECT_BLOCKS + INDIRECT_BLOCKS) > *index) &&
        (0 != *sector))
  {
    size_t free_blocks = *sector < SECTORS_PER_IBLOCK ?  *sector : SECTORS_PER_IBLOCK;
    block_sector_t block[SECTORS_PER_IBLOCK];
    block_read(fs_device, inode->blocks[*index], &block);

    for (size_t i = 0; i < free_blocks; i++)
    {
      free_map_release (block[i], 1);
      block[i] = 0;
      (*sector)--;
    }

    free_map_release (inode->blocks[*index], 1);
    inode->blocks[*index] = 0;
    (*index)++;
  }
}

static void free_dbl_indirect (struct inode *inode, size_t *sector)
{
  if ((INODE_BLOCKS - 1) == inode->direct_idx)
  {
    block_sector_t level_one[SECTORS_PER_IBLOCK];
    block_sector_t level_two[SECTORS_PER_IBLOCK];

    /* Read first level block */
    block_read (fs_device, inode->blocks[INODE_BLOCKS - 1], &level_one);

    /* Calculate # of indirect blocks */
    size_t indirect_blocks = DIV_ROUND_UP (*sector, SECTORS_PER_IBLOCK * BLOCK_SECTOR_SIZE);

    for (size_t i = 0; i < indirect_blocks; i++)
    {
      size_t free_blocks = *sector < SECTORS_PER_IBLOCK ? *sector : SECTORS_PER_IBLOCK;
      
      /* Read second level block */
      block_read (fs_device, level_one[i], &level_two);

      for (size_t j = 0; j < free_blocks; j++)
      {
        /* Free sectors */
        free_map_release (level_two[j], 1);
        level_two[j] = 0;
        (*sector)--;
      }

      /* Free second level block */
      free_map_release (level_one[i], 1);
      level_one[i] = 0;
    }

    if (0 < indirect_blocks)
      block_write (fs_device, inode->blocks[INODE_BLOCKS - 1], &level_one);

    /* Free first level block */
    free_map_release (inode->blocks[INODE_BLOCKS - 1], 1);
    inode->blocks[INODE_BLOCKS - 1] = 0;
  }
}

static void inode_release (struct inode *inode)
{
  size_t sector_num = bytes_to_sectors(inode->length);
  size_t idx = 0;

  if (0 == sector_num)
    return;

  /* Free direct blocks (index < 4) */
  free_direct (&inode, &sector_num, &idx);

  /* Free indirect blocks (index < 13) */
  free_indirect (inode, &sector_num, &idx);

  /* Free double indirect blocks (index 13) */
  free_dbl_indirect (inode, &sector_num);
}

bool inode_is_dir (const struct inode *inode) { return inode->is_dir; }

int inode_get_open_cnt (const struct inode *inode) { return inode->open_cnt; }

void set_total_sectors (size_t sectors) { total_device_sectors = sectors; }
bool is_valid_sector (block_sector_t sector) { return sector > 0 && sector < total_device_sectors; }

size_t inode_get_physical_size (const struct inode *inode)
{
  size_t size = 0;
  for (size_t i = 0; i < DIRECT_BLOCKS; i++)
  {
    if (0 != inode->blocks[i])
      size += BLOCK_SECTOR_SIZE;
  }

  return size;
}

size_t inode_get_block_cnt (const struct inode *inode)
{
  size_t count = 0;
  for (size_t i = 0; i < DIRECT_BLOCKS; i++)
  {
    if (0 != inode->blocks[i])
      count++;
  }

  return count;
}

block_sector_t inode_get_parent (const struct inode *inode) { return inode->parent; }

bool inode_set_parent (block_sector_t parent, block_sector_t child)
{
  struct inode* inode = inode_open (child);
  if (!inode)
    return false;

  inode->parent = parent;
  inode_close (inode);
  return true;
}

void inode_lock (const struct inode *inode) { lock_acquire(&((struct inode *)inode)->lock); }

void inode_unlock (const struct inode *inode) { lock_release(&((struct inode *) inode)->lock); }
