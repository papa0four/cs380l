#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
<<<<<<< HEAD

#define WORDSZ 4
=======
>>>>>>> f9b93c36c52a54dfbfcce5d528f35e7454d9c996

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

<<<<<<< HEAD
#endif /* userprog/process.h */
=======
#endif /* userprog/process.h */
>>>>>>> f9b93c36c52a54dfbfcce5d528f35e7454d9c996
