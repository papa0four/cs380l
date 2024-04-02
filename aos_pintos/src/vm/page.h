#include <hash.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include "filesys/file.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "filesys/off_t.h"
#include "devices/block.h"

/* Defined for uninitialized sector values */
#define BLOCK_SECTOR_NONE ((block_sector_t) -1)

/* a supplemental page table entry */
struct spt_entry
{
    void *vaddr;                /* Virtual address */
    bool is_loaded;             /* True if the page is loaded into memory */
    struct file *file;          /* Pointer to the file this page is loaded from, NULL otherwise */
    off_t file_offset;          /* Offset in the file to read the data from */
    uint32_t read_bytes;        /* Number of bytes read from file */
    uint32_t zero_bytes;        /* Number of bytes to zero after reading */
    size_t swap_index;          /* Swap slot index if the page is in swap */
    bool writable;              /* True if the page is writable */
    struct hash_elem hash_elem; /* Hash table element */
    struct thread *thread;      /* Pointer to the process that owns the page */
    struct frame *frame;        /* Pointer to the physical frame, NULL if not loaded */
    block_sector_t sector;      /* Starting sector on the swap device where page is stored */
    bool private;               /* True if memory-mapped file pages can be written back to their original file */
    off_t file_bytes;           /* Specifies the number of bytes to be read or written from a file */
};

struct spt_entry *spt_entry_create (void *vaddr, bool writable);