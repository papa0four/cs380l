#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <stdbool.h>

struct spt_entry;

void swap_init (void);
void swap_in (struct spt_entry *p);
bool swap_out (struct spt_entry *p);

#endif /* VM_SWAP_H */