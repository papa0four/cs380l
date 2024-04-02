#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include <stdbool.h>

#define MAX_STACK_SZ (8 * 1024 * 1024) // 8 MB stack size limit
#define STACK_THRESHOLD (4 * PGSIZE) // 4 pages worth of growth

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

/* User Implmented: shared by exception.c */
bool is_stack_access (void *esp, void *addr);
bool expand_stack (void **esp, void *addr);

#endif /* userprog/process.h */
