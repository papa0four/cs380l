#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
// #include <type_traits>
#include "devices/block.h"
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "filesys/off_t.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* MACROS for on-disk inode enhancements */
#define DIRECT_BLOCK_PTR 10
#define INDIRECT_BLOCK_PTR 1
#define DBL_INDIRECT_BLOCK_PTR 1
#define BLOCK_PTR_SIZE sizeof (block_sector_t)
#define SECTORS_PER_IBLOCK (BLOCK_SECTOR_SIZE / sizeof (block_sector_t))

/* Math behind padding
  direct ptr = 10 * 4 = 40
  indirect ptr = 1 * 4 = 4
  dbl indirect = 1 * 4 = 4
  length + magic + is_symlink = 4 + 4 + 4 = 12
  direct + indirect + dbl indirect = 40 + 4 + 4 = 48
  sum = 12 + 48 = 60
  BLK SZ - sum = 512 - 60 = 452
  number of uint32_t elements needed = 452 / 4 = 113
*/
#define PADDING 113

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
{
  // block_sector_t start; /* First data sector. */
  off_t length;         /* File size in bytes. */
  unsigned magic;       /* Magic number. */

  /*User additions/modifications */
  block_sector_t direct[DIRECT_BLOCK_PTR];            /* Used to handle direct pointers */
  block_sector_t indirect[INDIRECT_BLOCK_PTR];        /* Used to handle indirect pointers */
  block_sector_t d_indirect[DBL_INDIRECT_BLOCK_PTR];   /* Used to handle doubly indirect pointers */
  uint32_t unused[PADDING];                           /* Padded to fix BLOCK_SECTOR_SIZE */
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
  int deny_write_cnt;     /* 0: writes ok, >0: deny writes. */
  struct inode_disk data; /* Inode content. */
};

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
// static block_sector_t byte_to_sector (const struct inode *inode, off_t pos)
// {
//   ASSERT (inode != NULL);
//   // if (pos < inode->data.length)
//   //   return inode->data.start + pos / BLOCK_SECTOR_SIZE;
//   // else
//   //   return -1;

//   /* User Modification */

//   if (pos >= inode->data.length)
//     return -1; // position out of bounds 

//   block_sector_t result = -1;
//   off_t index = pos / BLOCK_SECTOR_SIZE;

//   if (DIRECT_BLOCK_PTR > index)
//     result = inode->data.direct[index];
//   else
//   {
//     index -= DIRECT_BLOCK_PTR; // adjust index for indirect blocks

//     if ((int) SECTORS_PER_IBLOCK > index)
//     {
//       block_sector_t indirect_block[SECTORS_PER_IBLOCK];
//       if (0 != inode->data.indirect[0]) // initialization check
//       {
//         block_read (fs_device, inode->data.indirect[0], &indirect_block);
//         result = indirect_block[index];
//       }
//     }
//     else
//     {
//       index -= SECTORS_PER_IBLOCK; // adjust index for doubly indirect blocks
//       if ((2 * (int) SECTORS_PER_IBLOCK) > index)
//       {
//         block_sector_t doubly_iblock[SECTORS_PER_IBLOCK];
//         if (0 != inode->data.d_indirect[0])
//         {
//           block_read (fs_device, inode->data.d_indirect[0], &doubly_iblock);
//           block_sector_t indirect_block[SECTORS_PER_IBLOCK];
//           off_t indirect_index = index / SECTORS_PER_IBLOCK;
//           off_t direct_index = index % SECTORS_PER_IBLOCK;
//           block_read (fs_device, doubly_iblock[indirect_index], &indirect_block);
//           result = indirect_block[direct_index];
//         }
//       }
//     }
//   }

//   return result;
// }

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void inode_init (void) { list_init (&open_inodes); }

static bool allocate_inode_blocks (struct inode_disk * inode, size_t sectors)
{
  static char zeros[BLOCK_SECTOR_SIZE];
  size_t i = 0;
  size_t j = 0;
  size_t k = 0;
  bool success = false;

  /* allocate direct blocks */
  for (i = 0; i < sectors && i < DIRECT_BLOCK_PTR; ++i)
  {
    if (!free_map_allocate (1, &inode->direct[i]))
      return false;

    block_write (fs_device, inode->direct[i], zeros);
  }

  /* allocate indirect blocks */
  if ((sectors > i) &&
      ((DIRECT_BLOCK_PTR + SECTORS_PER_IBLOCK) > i))
  {
    block_sector_t indirect_blocks[SECTORS_PER_IBLOCK];
    if (!free_map_allocate (1, &inode->indirect[0]))
      return false;

    for (j = 0; j < SECTORS_PER_IBLOCK && i < sectors; ++j, ++i)
    {
      if (!free_map_allocate (1, &indirect_blocks[j]))
        return false;
      block_write (fs_device, indirect_blocks[j], zeros);
    }

    block_write (fs_device, inode->indirect[0], indirect_blocks);
  }

  /* allocate doubly indirect blocks */
  if (sectors > i)
  {
    block_sector_t doubly_iblocks[SECTORS_PER_IBLOCK];
    if (!free_map_allocate (1, &inode->d_indirect[0]))
      return false;

    for (j = 0; j < SECTORS_PER_IBLOCK && i < sectors; ++j)
    {
      block_sector_t indirect_blocks[SECTORS_PER_IBLOCK];
      if (!free_map_allocate (1, &doubly_iblocks[j]))
        return false;

      for (k = 0; k < SECTORS_PER_IBLOCK && i < sectors; ++k, ++i)
      {
        if (!free_map_allocate (1, &indirect_blocks[k]))
          return false;

        block_write (fs_device, indirect_blocks[k], zeros);
      }
      block_write (fs_device, doubly_iblocks[j], indirect_blocks);
    }
    block_write (fs_device, inode->d_indirect[0], doubly_iblocks);
  }

  success = true;
  return success;
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool inode_create (block_sector_t sector, off_t length)
{
  // struct inode_disk *disk_inode = NULL;
  // bool success = false;

  // ASSERT (length >= 0);

  // /* If this assertion fails, the inode structure is not exactly
  //    one sector in size, and you should fix that. */
  // ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);
// k_inode->indirect[0] != 0) {
                
  //   {
  //     size_t sectors = bytes_to_sectors (length);
  //     disk_inode->length = length;
  //     disk_inode->magic = INODE_MAGIC;
  //     disk_inode->is_symlink = false;
  //     if (free_map_allocate (sectors, &disk_inode->start))
  //       {
  //         block_write (fs_device, sector, disk_inode);
  //         if (sectors > 0)
  //           {
  //             static char zeros[BLOCK_SECTOR_SIZE];
  //             size_t i;

  //             for (i = 0; i < sectors; i++)
  //               block_write (fs_device, disk_inode->start + i, zeros);
  //           }
  //         success = true;
  //       }
  //     free (disk_inode);
  //   }
  // return success;
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (0 <= length);

  disk_inode = malloc (sizeof (struct inode_disk));
  if (NULL == disk_inode)
    /* malloc failed */
    return success;

  disk_inode->length = length;
  disk_inode->magic = INODE_MAGIC;
  disk_inode->is_symlink = false;

  memset (disk_inode->direct, 0, sizeof (disk_inode->direct));
  memset (disk_inode->indirect, 0, sizeof (disk_inode->indirect));
  memset (disk_inode->d_indirect, 0, sizeof (disk_inode->d_indirect));

  /* write inode to disk */
  block_write (fs_device, sector, disk_inode);

  /* allocate and zero-fill blocks if fsize if > 0 */
  if (0 < length)
  {
    size_t sectors = bytes_to_sectors(length);
    if (allocate_inode_blocks (disk_inode, sectors))
      success = true;
  }
  else
     success = true; // empty file creation
  
  block_write (fs_device, sector, disk_inode);
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

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  block_read (fs_device, inode->sector, &inode->data);
  return inode;
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

/* Closes INODE and writes it to disk. (Does it?  Check code.)
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void inode_close (struct inode *inode)
{
  // /* Ignore null pointer. */
  // if (inode == NULL)
  //   return;

  // /* Release resources if this was the last opener. */
  // if (--inode->open_cnt == 0)
  //   {
  //     /* Remove from inode list and release lock. */
  //     list_remove (&inode->elem);

  //     /* Deallocate blocks if removed. */
  //     if (inode->removed)
  //       {
  //         free_map_release (inode->sector, 1);
  //         free_map_release (inode->data.start,
  //                           bytes_to_sectors (inode->data.length));
  //       }

  //     free (inode);
  //   }

  /* Ignore the null pointer */
  if (NULL == inode)
    return;

  /* Release resources if this was the last opener */
  if (0 == --inode->open_cnt)
  {
    list_remove (&inode->elem);

    /* Deallocate blocks if removed */
    if (inode->removed)
    {
      size_t i;
      struct inode_disk *disk_inode = &inode->data;

      /* Deallocate direct blocks */
      for (i = 0; i < DIRECT_BLOCK_PTR; i++)
      {
        if (0 != disk_inode->direct[i])
          free_map_release (disk_inode->direct[i], 1);
      }

      /* Deallocate indirect blocks */
      if (0 != disk_inode->indirect[0])
      {
        block_sector_t indirect_blocks[SECTORS_PER_IBLOCK];
        block_read (fs_device, disk_inode->indirect[0], &indirect_blocks);
        for (i = 0; i < SECTORS_PER_IBLOCK; i++)
        {
          if (0 != indirect_blocks[i])
            free_map_release (indirect_blocks[i], 1);
        }

        free_map_release (disk_inode->indirect[0], 1);
      }

      /* deallocate doubly indirect blocks */
      if (0 != disk_inode->d_indirect[0])
      {
        block_sector_t d_iblocks[SECTORS_PER_IBLOCK];
        block_sector_t indirect_blocks[SECTORS_PER_IBLOCK];
        block_read (fs_device, disk_inode->d_indirect[0], &d_iblocks);
        for (i = 0; i < SECTORS_PER_IBLOCK; i++)
        {
          if (0 != d_iblocks[i])
          {
            block_read (fs_device, d_iblocks[i], &indirect_blocks);
            for (size_t j = 0; j < SECTORS_PER_IBLOCK; j++)
            {
              if (0 != indirect_blocks[j])
                free_map_release (indirect_blocks[j], 1);
            }

            free_map_release (d_iblocks[i], 1);
          }
        }

        free_map_release  (disk_inode->d_indirect[0], 1);
      }

      /* Release the inode's sector */
      free_map_release (inode->sector, 1);
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

static block_sector_t fetch_block_from_doubly (block_sector_t d_indirect_ptr, off_t index)
{
  if (0 == d_indirect_ptr)
    return -1;

  block_sector_t indirect_blocks[SECTORS_PER_IBLOCK];
  block_read (fs_device, d_indirect_ptr, &indirect_blocks);

  off_t indirect_index = index / SECTORS_PER_IBLOCK;
  off_t direct_index = index % SECTORS_PER_IBLOCK;

  if (0 == indirect_blocks[indirect_index])
    return -1;

  block_sector_t direct_blocks[SECTORS_PER_IBLOCK];
  block_read (fs_device, indirect_blocks[indirect_index], &direct_blocks);

  return direct_blocks[direct_index];
}

static block_sector_t get_block_sector (const struct inode_disk *disk_inode, off_t pos)
{
  off_t index = pos / BLOCK_SECTOR_SIZE;
  if (pos >= disk_inode->length)
    return -1;

  if (DIRECT_BLOCK_PTR > index)
    return disk_inode->direct[index];
  else if ((int) (DIRECT_BLOCK_PTR + SECTORS_PER_IBLOCK)  > index)
  {
    index -= DIRECT_BLOCK_PTR;
    block_sector_t indirect_blocks[SECTORS_PER_IBLOCK];
    block_read (fs_device, disk_inode->indirect[0], &indirect_blocks);
    return indirect_blocks[index];
  }
  else
  {
    index -= (DIRECT_BLOCK_PTR + SECTORS_PER_IBLOCK);
    return fetch_block_from_doubly(disk_inode->d_indirect[0], index);
  }
}

static bool extend_file (struct inode *inode, off_t offset)
{
  static char zeros[BLOCK_SECTOR_SIZE];
  struct inode_disk *disk_inode = &inode->data;
  size_t needed_index = offset / BLOCK_SECTOR_SIZE;
  bool success = false;

  block_sector_t *sector_ptr = NULL;
  block_sector_t indirect_block[SECTORS_PER_IBLOCK];
  block_sector_t d_iblocks[SECTORS_PER_IBLOCK];

  if (DIRECT_BLOCK_PTR > needed_index) // handle direct blocks
  {
    sector_ptr = &disk_inode->direct[needed_index];
    if (0 == *sector_ptr)
    {
      success = free_map_allocate (1, sector_ptr);
      if (success)
        block_write (fs_device, *sector_ptr, zeros);
    }
  }
  else if ((DIRECT_BLOCK_PTR + SECTORS_PER_IBLOCK) > needed_index) // handle indirect blocks
  {
    needed_index -= DIRECT_BLOCK_PTR;
    if (0 == disk_inode->indirect[0])
    {
      success = free_map_allocate (1, &disk_inode->indirect[0]);
      if (!success)
        return false;

      memset (indirect_block, 0, sizeof (indirect_block));
    }
    else
      block_read (fs_device, disk_inode->indirect[0], &indirect_block);

    sector_ptr = &indirect_block[needed_index];
    if (0 == *sector_ptr)
    {
      success = free_map_allocate (1, sector_ptr);
      if (success)
      {
        block_write (fs_device, *sector_ptr, zeros);
        block_write (fs_device, disk_inode->indirect[0], indirect_block);
      }
    }
  }
  else // handle doubly indirect blocks
  {
    needed_index -= (DIRECT_BLOCK_PTR + SECTORS_PER_IBLOCK);
    size_t outer_index = needed_index / SECTORS_PER_IBLOCK;
    size_t inner_index = needed_index % SECTORS_PER_IBLOCK;

    if (0 == disk_inode->d_indirect[0])
    {
      success = free_map_allocate (1, &disk_inode->d_indirect[0]);
      if (!success)
        return false;

      memset (d_iblocks, 0, sizeof (d_iblocks));
    }
    else
      block_read (fs_device, disk_inode->d_indirect[0], &d_iblocks);

    if (0 == d_iblocks[outer_index])
    {
      success = free_map_allocate (1, &d_iblocks[outer_index]);
      if (!success)
        return false;

      memset (indirect_block, 0, sizeof (indirect_block));
      block_write (fs_device, d_iblocks[outer_index], &indirect_block);
    }
    else
      block_read (fs_device, d_iblocks[outer_index], &indirect_block);

    sector_ptr = &indirect_block[inner_index];
    if (0 == *sector_ptr)
    {
      success = free_map_allocate (1, sector_ptr);
      if (success)
      {
        block_write (fs_device, *sector_ptr, zeros);
        block_write (fs_device, d_iblocks[outer_index], indirect_block);
        block_write (fs_device, disk_inode->d_indirect[0], d_iblocks);
      }
    }
  }

  /* Update inode length if needed */
  if ((success) && (offset >= disk_inode->length))
  {
    disk_inode->length = offset + 1;
    block_write (fs_device, inode->sector, disk_inode);
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
  uint8_t *bounce = NULL;

  while (size > 0)
  {
    // /* Disk sector to read, starting byte offset within sector. */
    // block_sector_t sector_idx = byte_to_sector (inode, offset);
    // int sector_ofs = offset % BLOCK_SECTOR_SIZE;

    // /* Bytes left in inode, bytes left in sector, lesser of the two. */
    // off_t inode_left = inode_length (inode) - offset;
    // int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    // int min_left = inode_left < sector_left ? inode_left : sector_left;

    // /* Number of bytes to actually copy out of this sector. */
    // int chunk_size = size < min_left ? size : min_left;
    // if (chunk_size <= 0)
    //   break;

    // if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
    // {
    //   /* Read full sector directly into caller's buffer. */
    //   block_read (fs_device, sector_idx, buffer + bytes_read);
    // }
    // else
    // {
    //   /* Read sector into bounce buffer, then partially copy
    //       into caller's buffer. */
    //   if (bounce == NULL)
    //   {
    //     bounce = malloc (BLOCK_SECTOR_SIZE);
    //     if (bounce == NULL)
    //       break;
    //   }
    //   block_read (fs_device, sector_idx, bounce);
    //   memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
    // }

    // /* Advance. */
    // size -= chunk_size;
    // offset += chunk_size;
    // bytes_read += chunk_size;
    block_sector_t sector_idx = get_block_sector (&inode->data, offset);
    if ((block_sector_t) -1 == sector_idx)
      break; // EOF or error

    int sector_ofs = offset % BLOCK_SECTOR_SIZE;
    int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    int min_left = inode->data.length - offset < sector_left ? inode->data.length - offset : sector_left;
    int chunk_size = size < min_left ? size : min_left;

    if (0 >= chunk_size)
      break;

    if (NULL == bounce)
    {
      bounce = malloc (BLOCK_SECTOR_SIZE);
      if (NULL == bounce)
        break;
    }

    block_read (fs_device, sector_idx, bounce);
    memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);

    size -= chunk_size;
    offset += chunk_size;
    bytes_read += chunk_size;
  }

  free (bounce);
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
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  while (size > 0)
  {
    // /* Sector to write, starting byte offset within sector. */
    // block_sector_t sector_idx = byte_to_sector (inode, offset);
    // int sector_ofs = offset % BLOCK_SECTOR_SIZE;

    // /* Bytes left in inode, bytes left in sector, lesser of the two. */
    // off_t inode_left = inode_length (inode) - offset;
    // int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    // int min_left = inode_left < sector_left ? inode_left : sector_left;

    // /* Number of bytes to actually write into this sector. */
    // int chunk_size = size < min_left ? size : min_left;
    // if (chunk_size <= 0)
    //   break;

    // if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
    // {
    //   /* Write full sector directly to disk. */
    //   block_write (fs_device, sector_idx, buffer + bytes_written);
    // }
    // else
    // {
    //   /* We need a bounce buffer. */
    //   if (bounce == NULL)
    //   {
    //     bounce = malloc (BLOCK_SECTOR_SIZE);
    //     if (bounce == NULL)
    //       break;
    //   }

    //   /* If the sector contains data before or after the chunk
    //       we're writing, then we need to read in the sector
    //       first.  Otherwise we start with a sector of all zeros. */
    //   if (sector_ofs > 0 || chunk_size < sector_left)
    //     block_read (fs_device, sector_idx, bounce);
    //   else
    //     memset (bounce, 0, BLOCK_SECTOR_SIZE);
    //   memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
    //   block_write (fs_device, sector_idx, bounce);
    // }

    // /* Advance. */
    // size -= chunk_size;
    // offset += chunk_size;
    // bytes_written += chunk_size;

    block_sector_t sector_idx = get_block_sector (&inode->data, offset);
    if ((block_sector_t) -1 == sector_idx)
    {
      if (!extend_file (inode, offset))
        break;

      sector_idx = get_block_sector (&inode->data, offset);
      if ((block_sector_t) -1 == sector_idx)
        break;
    }

    int sector_ofs = offset % BLOCK_SECTOR_SIZE;
    int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    int min_left = inode->data.length - offset < sector_left ? inode->data.length - offset : sector_left;
    int chunk_size = size < min_left ? size : min_left;

    if (0 >= chunk_size)
      break;

    if (NULL == bounce)
    {
      bounce = malloc (BLOCK_SECTOR_SIZE);
      if (NULL == bounce)
        break;
    }

    if ((0 < sector_ofs) || (chunk_size < sector_left))
      block_read (fs_device, sector_idx, bounce);
    else
      memset (bounce, 0, BLOCK_SECTOR_SIZE);

    memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
    block_write (fs_device, sector_idx, bounce);

    size -= chunk_size;
    offset += chunk_size;
    bytes_written += chunk_size;
  }

  free (bounce);
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
off_t inode_length (const struct inode *inode) { return inode->data.length; }

bool inode_get_symlink (struct inode *inode) { 
  ASSERT (inode != NULL);
  return inode->data.is_symlink; 
}

void inode_set_symlink (struct inode *inode, bool is_symlink)
{
  inode->data.is_symlink = is_symlink;
  block_write (fs_device, inode->sector, &inode->data);
}
