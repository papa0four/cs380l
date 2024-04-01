#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include <stdbool.h>

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

/* User Implmented: shared by exception.c */
bool is_stack_access (void *esp, void *addr);
bool expand_stack (void **esp, void *addr);

#endif /* userprog/process.h */
