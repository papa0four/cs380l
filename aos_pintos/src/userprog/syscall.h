#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

typedef int pid_t;

<<<<<<< HEAD
struct lock filesys_lock;

void syscall_init (void);
=======
struct child_process
{
  int pid;                        /* the PID of the child process */
  int load_status;                /* status of file loading */
  int wait;                       /* 0/1 to determine if process is in wait status */
  int exit;                       /* 0/1 to determine if process is in exit status */
  int status;                     /* process EXIT status value */
  struct semaphore load_sema;     /* semaphore used for file load */
  struct semaphore exit_sema;     /* semaphore used for file exit */
  struct list_elem elem;          /* list element for child list (thread.h) */
};

struct process_file
{
    struct file *file;            /* pointer to the process' file struct (file.h) */
    int fd;                       /* process file's file descriptor */
    struct list_elem elem;        /* list element for file list (thread.h) */
};

struct lock file_system_lock;     /* global lock for locking file system operations */

/* User defined helper functions */
struct child_process  *child_process_find (int pid);
struct file           *file_get (int fd);
void                   child_process_remove (struct child_process *child);
void                   child_process_remove_all (void);
void                   process_file_close (int fd);
void                   syscall_exit (int status);
int                    translate_vaddr (const void *vaddr);
>>>>>>> f9b93c36c52a54dfbfcce5d528f35e7454d9c996

#endif /* userprog/syscall.h */