<<<<<<< HEAD
#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdint.h>
#include <hash.h>
#include "devices/block.h"
#include "threads/palloc.h"
#include "filesys/file.h"
#include "vm/frame.h"
#include "threads/synch.h"

#define SECTOR_INIT ((size_t) -1)
#define STACK_LIMIT (4 * BLOCK_SECTOR_SIZE)
#define PUSHA 32

/* struct and enum declarations */
enum page_status
  {
    ZERO_FILL,         /* Uninitialized page */
    FRAME_MAPPED,      /* Mapped in RAM */
    SWAP_MAPPED,       /* Stored in swap */
    FILE_MAPPED        /* Backed by file */
  };

/* page struct with hash element (hash_elem) */
/* shared between page.c process.c load_segment exception.c */
struct spt_entry
{
  struct hash_elem hash_elem; /* Hash table element. */
  void *addr;                 /* Virtual address. */
  /* ...other members... */
  struct frame_entry *frame;  /* Occupied frame, if any */
  enum page_status status;    /* Where the page should be */
  bool is_stack_page;         /* If this is a stack page */
  bool writable;              /* Can you write to this page */
  struct file *file;          /* File to load page from */
  off_t offset;               /* File offset */
  size_t read_bytes;          /* Number of bytes to read from file */
  int sector;                 /* Sector in the swap table */
  uint32_t *pagedir;          /* Owner thread's pagedir */
};

unsigned spt_hash_func (const struct hash_elem *, void *);
bool spt_less_func (const struct hash_elem *, const struct hash_elem *, void *);
void page_destructor (struct hash_elem *, void *);
struct spt_entry *page_lookup (void *);

#endif /* vm/page.h */
=======
>>>>>>> f9b93c36c52a54dfbfcce5d528f35e7454d9c996
