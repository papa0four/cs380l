#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include <stdbool.h>
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
#define DIRECT_PTR 4
#define INDIRECT_PTR 9
#define DBL_INDIRECT_PTR 1
#define INODE_BLOCKS 14
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
  424 / 4 = 106
*/
#define PADDING 106

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
{
  // block_sector_t start; /* First data sector. */
  off_t length;         /* File size in bytes. */
  unsigned magic;       /* Magic number. */

  /*User additions/modifications */
  block_sector_t blocks[INODE_BLOCKS]; /* Block pointers DIRECT/INDIRECT/DBLY INDIRECT */
  uint32_t direct_idx;                 /* Used to handle direct pointers */
  uint32_t indirect_idx;               /* Used to handle indirect pointers */
  uint32_t d_indirect_idx;             /* Used to handle doubly indirect pointers */
  uint32_t unused[PADDING];            /* Modified - Padded to fix BLOCK_SECTOR_SIZE */
  bool is_dir;                         /* flag to determine file vs directory */
  block_sector_t parent;               /* parent block sector */
  /* End user additions/modifications */

  bool is_symlink;      /* True if symbolic link, false otherwise. */
};

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode
{
  struct list_elem elem;  /* Element in inode list. */
  block_sector_t sector;  /* Sector number of disk location. */
  int open_cnt;           /* Number of openers. */
  bool removed;           /* True if deleted, false otherwise. */
  int deny_write_cnt;     /* 0: writes ok, > 0: deny writes. */
  struct inode_disk data; /* Inode content. */

  /* User additions */
  struct lock lock;                     /* inode lock for writing files */
  off_t length;                         /* file size in bytes */
  block_sector_t blocks[INODE_BLOCKS];  /* Block pointers DIRECT/INDIRECT/DBLY INDIRECT*/
  uint32_t read_length;                 /* bytes that can be read from file */
  uint32_t direct_idx;                  /* current index for next direct block */
  uint32_t indrect_idx;                 /* current index for next indirect block */
  uint32_t d_indrect_idx;               /* current index for next double indirect block */
  bool is_dir;                          /* flag to determine if inode is directory */
  bool is_symlink;                      /* flag to determine if inode is symlink */
  block_sector_t parent;                /* maintain parent sector */
  /* End user additions */
};

/* User defined helper functions */
static bool inode_allocate (struct inode_disk *disk_inode);
static bool handle_indirect (block_sector_t *indirect_block, 
                            block_sector_t *indirect_sector,
                            size_t index);
static bool handle_dbl_indirect (block_sector_t *d_iblocks,
                                block_sector_t *d_isector,
                                size_t index);
static bool inode_extend (struct inode *inode, off_t offset);
static void inode_release (struct inode *inode);

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t byte_to_sector (const struct inode *inode, off_t length, off_t pos)
{
  ASSERT (inode != NULL);
  if (length > pos)
  {
    uint32_t idx = 0;
    uint32_t blocks[SECTORS_PER_IBLOCK];

    /* Handle direct blocks */
    if ((DIRECT_PTR * BLOCK_SECTOR_SIZE) > pos)
      return inode->blocks[pos / BLOCK_SECTOR_SIZE];

    /* Handle indirect blocks */
    else if ((DIRECT_PTR + INDIRECT_PTR * SECTORS_PER_IBLOCK) * BLOCK_SECTOR_SIZE)
    {
      pos -= DIRECT_PTR * BLOCK_SECTOR_SIZE;
      idx = pos / (SECTORS_PER_IBLOCK * BLOCK_SECTOR_SIZE) + DIRECT_PTR;
      block_read (fs_device, inode->blocks[idx], &blocks);

      pos %= SECTORS_PER_IBLOCK * BLOCK_SECTOR_SIZE;
      return blocks[pos / BLOCK_SECTOR_SIZE];
    }

    /* Handle doubly indirect blocks */
    else
    {
      block_read (fs_device, inode->blocks[INODE_BLOCKS - 1], &blocks);

      pos -= (DIRECT_PTR + INDIRECT_PTR * SECTORS_PER_IBLOCK) * BLOCK_SECTOR_SIZE;
      idx = pos / (SECTORS_PER_IBLOCK * BLOCK_SECTOR_SIZE);
      block_read (fs_device, blocks[idx], &blocks);

      pos %= SECTORS_PER_IBLOCK * BLOCK_SECTOR_SIZE;
      return blocks[pos / BLOCK_SECTOR_SIZE];
    }
  }
  else
    return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void inode_init (void) { list_init (&open_inodes); }

// static bool allocate_inode_blocks (struct inode_disk * inode, size_t sectors)
// {
//   static char zeros[BLOCK_SECTOR_SIZE];
//   size_t i = 0;
//   size_t j = 0;
//   size_t k = 0;
//   bool success = false;

//   /* allocate direct blocks */
//   for (i = 0; i < sectors && i < DIRECT_BLOCK_PTR; ++i)
//   {
//     if (!free_map_allocate (1, &inode->direct[i]))
//       return false;

//     block_write (fs_device, inode->direct[i], zeros);
//   }

//   /* allocate indirect blocks */
//   if ((sectors > i) &&
//       ((DIRECT_BLOCK_PTR + SECTORS_PER_IBLOCK) > i))
//   {
//     block_sector_t indirect_blocks[SECTORS_PER_IBLOCK];
//     if (!free_map_allocate (1, &inode->indirect[0]))
//       return false;

//     for (j = 0; j < SECTORS_PER_IBLOCK && i < sectors; ++j, ++i)
//     {
//       if (!free_map_allocate (1, &indirect_blocks[j]))
//         return false;
//       block_write (fs_device, indirect_blocks[j], zeros);
//     }

//     block_write (fs_device, inode->indirect[0], indirect_blocks);
//   }

//   /* allocate doubly indirect blocks */
//   if (sectors > i)
//   {
//     block_sector_t doubly_iblocks[SECTORS_PER_IBLOCK];
//     if (!free_map_allocate (1, &inode->d_indirect[0]))
//       return false;

//     for (j = 0; j < SECTORS_PER_IBLOCK && i < sectors; ++j)
//     {
//       block_sector_t indirect_blocks[SECTORS_PER_IBLOCK];
//       if (!free_map_allocate (1, &doubly_iblocks[j]))
//         return false;

//       for (k = 0; k < SECTORS_PER_IBLOCK && i < sectors; ++k, ++i)
//       {
//         if (!free_map_allocate (1, &indirect_blocks[k]))
//           return false;

//         block_write (fs_device, indirect_blocks[k], zeros);
//       }
//       block_write (fs_device, doubly_iblocks[j], indirect_blocks);
//     }
//     block_write (fs_device, inode->d_indirect[0], doubly_iblocks);
//   }

//   success = true;
//   return success;
// }

static bool inode_allocate (struct inode_disk *disk_inode)
{
  struct inode inode;
  inode.length        = 0;
  inode.direct_idx    = 0;
  inode.indrect_idx   = 0;
  inode.d_indrect_idx = 0;
  inode.data          = 

  inode_extend (&inode, disk_inode->length);
  
  disk_inode->direct_idx = inode.direct_idx;
  disk_inode->indirect_idx = inode.indrect_idx;
  disk_inode->d_indirect_idx = inode.d_indrect_idx;
  
  memcpy (&disk_inode->blocks, &inode.blocks, INODE_BLOCKS * BLOCK_PTR_SIZE);
  return true;
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool inode_create (block_sector_t sector, off_t length, bool is_dir)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (0 <= length);
  ASSERT (BLOCK_SECTOR_SIZE == sizeof (*disk_inode));

  disk_inode = calloc (1, sizeof (struct inode_disk));
  if (NULL == disk_inode)
    /* malloc failed */
    return success;

  disk_inode->length      = length;
  disk_inode->magic       = INODE_MAGIC;
  disk_inode->is_symlink  = false;
  disk_inode->is_dir      = is_dir;
  disk_inode->parent      = ROOT_DIR_SECTOR;
  
  if (inode_allocate (disk_inode))
  {
    block_write (fs_device, sector, disk_inode);
    success = true;
  }

  free (disk_inode);
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
    if (inode->sector == sector)
    {
      inode_reopen (inode);
      return inode;
    }
  }

  /* Allocate inode memory */
  inode = malloc (sizeof (*inode));
  if (NULL == inode)
    return NULL;

  struct inode_disk disk_inode;

  list_push_front (&open_inodes, &inode->elem);
  inode->sector         = sector;
  inode->open_cnt       = 1;
  inode->deny_write_cnt = 0;
  inode->removed        = false;
  lock_init (&inode->lock);

  /* Copy data on disk to inode */
  block_read (fs_device, inode->sector, &disk_inode);

  inode->length         = disk_inode.length;
  inode->read_length    = disk_inode.length;
  inode->direct_idx     = disk_inode.direct_idx;
  inode->indrect_idx    = disk_inode.indirect_idx;
  inode->d_indrect_idx  = disk_inode.d_indirect_idx;
  inode->is_dir         = disk_inode.is_dir;
  inode->is_symlink     = disk_inode.is_symlink;
  inode->parent         = disk_inode.parent;

  memcpy (&inode->blocks, &disk_inode.blocks, INODE_BLOCKS * BLOCK_PTR_SIZE);
  return inode;

  // /* Allocate memory. */
  // inode = malloc (sizeof *inode);
  // if (inode == NULL)
  //   return NULL;

  // /* Initialize. */
  // list_push_front (&open_inodes, &inode->elem);
  // inode->sector = sector;
  // inode->open_cnt = 1;
  // inode->deny_write_cnt = 0;
  // inode->removed = false;
  // block_read (fs_device, inode->sector, &inode->data);
  // return inode;

}

/* Reopens and returns INODE. */
struct inode *inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

static void release_indirect (block_sector_t indirect_sector)
{
  if (0 == indirect_sector)
    return;

  block_sector_t indirect_blocks[SECTORS_PER_IBLOCK];
  block_read (fs_device, indirect_sector, &indirect_blocks);

  for (size_t i = 0; i < SECTORS_PER_IBLOCK; i++)
  {
    if (0 != indirect_blocks[i])
      free_map_release (indirect_blocks[i], 1);
  }

  free_map_release (indirect_sector, 1);
}

static void release_dbl_indirect (block_sector_t d_isector)
{
  if (0 == d_isector)
    return;

  block_sector_t level_one[SECTORS_PER_IBLOCK];
  block_read (fs_device, d_isector, &level_one);

  for (size_t i = 0; i < SECTORS_PER_IBLOCK; i++)
  {
    if (0 != level_one[i])
      release_indirect (level_one[i]);
  }

  free_map_release (d_isector, 1);
}

static void inode_release (struct inode *inode)
{
  if (0 == inode->length)
    return;

  size_t sectors = bytes_to_sectors (inode->length);

  /* Free direct blocks */
  for (int i = 0; (DIRECT_PTR > i) && (0 < sectors); i++, sectors--)
  {
    if (0 != inode->blocks[i])
      free_map_release (inode->blocks[i], 1);
  }

  /* Free indirect blocks */
  if ((0 < sectors) && (0 != inode->indrect_idx))
  {
    release_indirect (inode->blocks[DIRECT_PTR]);
    sectors -= SECTORS_PER_IBLOCK;
  }

  if ((0 < sectors) && (0 != inode->d_indrect_idx))
    release_dbl_indirect (inode->blocks[DIRECT_PTR + 1]);
}

/* Closes INODE and writes it to disk. (Does it?  Check code.)
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void inode_close (struct inode *inode)
{
  // /* Ignore the null pointer */
  // if (NULL == inode)
  //   return;

  // /* Release resources if this was the last opener */
  // if (0 == --inode->open_cnt)
  // {
  //   list_remove (&inode->elem);

  //   /* Deallocate blocks if removed */
  //   if (inode->removed)
  //   {
  //     size_t i;
  //     struct inode_disk *disk_inode = &inode->data;

  //     /* Deallocate direct blocks */
  //     for (i = 0; i < DIRECT_BLOCK_PTR; i++)
  //     {
  //       if (0 != disk_inode->direct[i])
  //         free_map_release (disk_inode->direct[i], 1);
  //     }

  //     /* Deallocate indirect blocks */
  //     if (0 != disk_inode->indirect[0])
  //     {
  //       block_sector_t indirect_blocks[SECTORS_PER_IBLOCK];
  //       block_read (fs_device, disk_inode->indirect[0], &indirect_blocks);
  //       for (i = 0; i < SECTORS_PER_IBLOCK; i++)
  //       {
  //         if (0 != indirect_blocks[i])
  //           free_map_release (indirect_blocks[i], 1);
  //       }

  //       free_map_release (disk_inode->indirect[0], 1);
  //     }

  //     /* deallocate doubly indirect blocks */
  //     if (0 != disk_inode->d_indirect[0])
  //     {
  //       block_sector_t d_iblocks[SECTORS_PER_IBLOCK];
  //       block_sector_t indirect_blocks[SECTORS_PER_IBLOCK];
  //       block_read (fs_device, disk_inode->d_indirect[0], &d_iblocks);
  //       for (i = 0; i < SECTORS_PER_IBLOCK; i++)
  //       {
  //         if (0 != d_iblocks[i])
  //         {
  //           block_read (fs_device, d_iblocks[i], &indirect_blocks);
  //           for (size_t j = 0; j < SECTORS_PER_IBLOCK; j++)
  //           {
  //             if (0 != indirect_blocks[j])
  //               free_map_release (indirect_blocks[j], 1);
  //           }

  //           free_map_release (d_iblocks[i], 1);
  //         }
  //       }

  //       free_map_release  (disk_inode->d_indirect[0], 1);
  //     }

  //     /* Release the inode's sector */
  //     free_map_release (inode->sector, 1);
  //   }

  //   free (inode);
  // }
  struct inode_disk disk_inode;

  /* Ignore null pointer. */
  if (NULL == inode)
    return;

  /* Release resources if this was the last opener. */
  if (0 == --inode->open_cnt)
  {
    /* Remove from list */
    list_remove (&inode->elem);

    /* Deallocate blocks if removed. */
    if (inode->removed)
    {
      free_map_release (inode->sector, 1);
      inode_release (inode);
    }
    /* Write back to disk */
    else
    {
      disk_inode.length         = inode->length;
      disk_inode.magic          = INODE_MAGIC;
      disk_inode.direct_idx     = inode->direct_idx;
      disk_inode.indirect_idx   = inode->indrect_idx;
      disk_inode.d_indirect_idx = inode->d_indrect_idx;
      disk_inode.is_dir         = inode->is_dir;
      disk_inode.is_symlink     = inode->is_symlink;
      disk_inode.parent         = inode->parent;

      memcpy (&disk_inode.blocks, &inode->blocks, INODE_BLOCKS * BLOCK_PTR_SIZE);
      block_write (fs_device, inode->sector, &disk_inode);
    }

    free (inode);
  }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void inode_remove (struct inode *inode)
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

static bool handle_indirect (block_sector_t *indirect_block, 
                            block_sector_t *indirect_sector,
                            size_t index)
{
  static char zeros[BLOCK_SECTOR_SIZE];
  if (0 == *indirect_sector)
  {
    if (!free_map_allocate (1, indirect_sector))
      return false;

    memset (indirect_block, 0, BLOCK_PTR_SIZE * SECTORS_PER_IBLOCK);
    /* Zero fill new indirect block */
    block_write (fs_device, *indirect_sector, zeros);
  }
  else
    block_read (fs_device, *indirect_sector, indirect_block);

  block_sector_t *sector_ptr = &indirect_block[index];
  if (0 == *sector_ptr)
  {
    if (!free_map_allocate (1, sector_ptr))
      return false;

    /* Zero fill newly allocated sector */
    block_write (fs_device, *sector_ptr, zeros);
  }

  /* Write updated indirect block to disk */
  block_write (fs_device, *indirect_sector, indirect_block);
  return true;
}

static bool handle_dbl_indirect (block_sector_t *d_iblocks,
                                block_sector_t *d_isector,
                                size_t index)
{
  static char zeros[BLOCK_SECTOR_SIZE];
  if (0 == *d_isector)
  {
    if (!free_map_allocate (1, d_isector))
      return false;

    memset (d_iblocks, 0, BLOCK_PTR_SIZE * SECTORS_PER_IBLOCK);
    /* Zero fill new doubly indirect block */
    block_write (fs_device, *d_isector, zeros);
  }
  else
    block_read (fs_device, *d_isector, d_iblocks);

  size_t outer_index = index / SECTORS_PER_IBLOCK;
  size_t inner_index = index % SECTORS_PER_IBLOCK;
  block_sector_t indirect_block[SECTORS_PER_IBLOCK];

  if (0 == d_iblocks[outer_index])
  {
    if (!free_map_allocate (1, &d_iblocks[outer_index]))
      return false;
    memset (indirect_block, 0, BLOCK_PTR_SIZE * SECTORS_PER_IBLOCK);
    block_write (fs_device, d_iblocks[outer_index], indirect_block);
  }
  else
    block_read (fs_device, d_iblocks[outer_index], indirect_block);

  if (0 == indirect_block[inner_index])
  {
    if (!free_map_allocate (1, &indirect_block[inner_index]))
      return false;

    /* Zero fill the newly allocated sector */
    block_write (fs_device, indirect_block[inner_index], zeros);
  }

  /* Write the updated indirect block to disk */
  block_write (fs_device, d_iblocks[outer_index], indirect_block);
  /* Write the updated doubly indirect block to disk */
  block_write (fs_device, *d_isector, d_iblocks);
  free (indirect_block);
  return true;
}

static bool inode_extend (struct inode *inode, off_t offset)
{
  static char zeros[BLOCK_SECTOR_SIZE];
  struct inode_disk *disk_inode = &inode->data;
  size_t needed_index = offset / BLOCK_SECTOR_SIZE;
  bool success = true;

  block_sector_t *sector_ptr = NULL;
  block_sector_t indirect_block[SECTORS_PER_IBLOCK];
  block_sector_t d_iblocks[SECTORS_PER_IBLOCK];

  /* Handle direct blocks */
  if (DIRECT_PTR > needed_index)
    sector_ptr = &disk_inode->blocks[needed_index];
  /* Handle indirect blocks */
  else if ((DIRECT_PTR + SECTORS_PER_IBLOCK) > needed_index)
  {
    needed_index -= DIRECT_PTR;
    if (!handle_indirect (indirect_block, &disk_inode->blocks[DIRECT_PTR], needed_index))
      success = false;
  }
  else
  {
    needed_index -= (DIRECT_PTR + SECTORS_PER_IBLOCK);
    if (!handle_dbl_indirect (d_iblocks, &disk_inode->blocks[DIRECT_PTR + 1], needed_index))
      success = false;
  }

  if ((success) && ((0 == *sector_ptr) || (offset >= disk_inode->length)))
  {
    if (0 == *sector_ptr)
    {
      success = free_map_allocate (1, sector_ptr);
      if (success)
        block_write (fs_device, *sector_ptr, zeros);
    }
    if (offset >= disk_inode->length)
      disk_inode->length = offset + 1;
  }

  return success;  
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t inode_read_at (struct inode *inode, void *buffer_, off_t size,
                     off_t offset)
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  off_t length = inode->read_length;

  while (size > 0)
  {
    block_sector_t sector_idx = byte_to_sector (inode, length, offset);
    if ((block_sector_t) -1 == sector_idx)
      break; // EOF or error

    int sector_ofs = offset % BLOCK_SECTOR_SIZE;
    off_t inode_left = length - offset;
    int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    int min_left = inode_left < sector_left ? inode_left : sector_left;
    int chunk_size = size < min_left ? size : min_left;

    if (0 >= chunk_size)
      break;

    int cache_idx = cache_access_entry (sector_idx, false);
    memcpy (buffer + bytes_read, cache[cache_idx].block + sector_ofs, chunk_size);
    cache[cache_idx].accessed = true;
    cache[cache_idx].open_cnt--;

    size -= chunk_size;
    offset += chunk_size;
    bytes_read += chunk_size;
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

  /* Check if beyond EOF, time to grow */
  if (inode_length (inode) < offset + size)
  {
    if (!inode->is_dir)
      lock_acquire (&inode->lock);

    inode->length = inode_extend (inode, offset + size);
    
    if (!inode->is_dir)
      lock_release (&inode->lock);
  }

  while (size > 0)
  {
    block_sector_t sector_idx = byte_to_sector (inode, inode_length (inode), offset);

    int sector_ofs    = offset % BLOCK_SECTOR_SIZE;
    off_t inode_left  = inode_length (inode) - offset;
    int sector_left   = BLOCK_SECTOR_SIZE - sector_ofs;
    int min_left      = inode_left < sector_left ? inode_left : sector_left;
    int chunk_size    = size < min_left ? size : min_left;

    if (0 >= chunk_size)
      break;

    int cache_idx = cache_access_entry (sector_idx, true);
    memcpy (cache[cache_idx].block + sector_ofs, buffer + bytes_written, chunk_size);

    cache[cache_idx].accessed = true;
    cache[cache_idx].dirty    = true;
    cache[cache_idx].open_cnt--;

    size -= chunk_size;
    offset += chunk_size;
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
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void inode_allow_write (struct inode *inode)
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t inode_length (const struct inode *inode) { return inode->length; }

bool inode_get_symlink (struct inode *inode) { 
  ASSERT (inode != NULL);
  return inode->is_symlink; 
}

void inode_set_symlink (struct inode *inode, bool is_symlink)
{
  inode->is_symlink = is_symlink;
  block_write (fs_device, inode->sector, &inode->blocks);
}
