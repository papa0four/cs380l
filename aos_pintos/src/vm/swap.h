<<<<<<< HEAD
#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "vm/page.h"

#define MAX_SECTORS 8
#define BLOCK_BITMAP_SIZE (2 * BLOCK_SECTOR_SIZE)

void swap_init (void);

void swap_in (struct spt_entry *);
void swap_out (struct spt_entry *);
void swap_free (struct spt_entry *);

#endif /* vm/swap.h */
=======
>>>>>>> f9b93c36c52a54dfbfcce5d528f35e7454d9c996
