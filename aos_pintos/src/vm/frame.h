#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <stdint.h>
#include "vm/page.h"

struct frame_entry
{
    uint32_t frame_id;        /* Unique Frame ID */
    void *kpage;              /* Frame's 'physical' address */
    struct spt_entry *page;   /* Pointer to page that occupies the frame */
};

void frame_init (size_t);
struct frame_entry *frame_get (void);
struct frame_entry *frame_get_multiple (size_t);
void frame_free (struct frame_entry *);

#endif /* vm/frame.h */