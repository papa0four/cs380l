#ifndef VM_PAHE_H
#define VM_PAGE_H

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
#define PUSHA_SZ 32
#define BLOCK_SECTOR_NONE ((block_sector_t) -1)

/* a supplemental page table entry */
struct spt_entry
{
    void *vaddr;                /* Virtual address */
    bool is_loaded;             /* True if the page is loaded into memory */
    struct file *file;          /* Pointer to the file this page is loaded from, NULL otherwise */
    off_t file_offset;          /* Offset in the file to read the data from */
    uint32_t zero_bytes;        /* Number of bytes to zero after reading */
    bool writable;              /* True if the page is writable */
    struct hash_elem hash_elem; /* Hash table element */
    struct thread *thread;      /* Pointer to the process that owns the page */
    struct frame *frame;        /* Pointer to the physical frame, NULL if not loaded */
    block_sector_t swap_index;  /* Starting sector on the swap device where page is stored */
    bool private;               /* True if memory-mapped file pages can be written back to their original file */
    off_t file_bytes;           /* Specifies the number of bytes to be read or written from a file */
};

void spt_destroy (void);
struct spt_entry *spt_entry_create (void *vaddr, bool writable);
void spt_entry_remove (void *vaddr);
struct spt_entry *spt_entry_lookup (const void *vaddr);
bool spt_page_in (void *faddr);
bool spt_page_out (struct spt_entry *page);
bool spt_recent (struct spt_entry *page);
bool spt_lock (const void *vaddr, bool writable);
void spt_unlock (const void *vaddr);

unsigned spt_entry_hash (const struct hash_elem *e, void *aux UNUSED);
bool spt_less_func (const struct hash_elem *a, const struct hash_elem *b);

#endif /* VM_PAGE_H */