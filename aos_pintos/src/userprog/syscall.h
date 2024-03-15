#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdbool.h>
#include <stdlib.h>
#include "threads/thread.h"
#include "threads/synch.h"

/* Process identifier. */
typedef int pid_t;
#define PID_ERROR ((pid_t) -1)

/* Maximum characters in a filename written by readdir(). */
#define READDIR_MAX_LEN 14

/* Typical return values from main() and arguments to exit(). */
#define EXIT_SUCCESS 0 /* Successful execution. */
#define EXIT_FAILURE 1 /* Unsuccessful execution. */

/* Value to conduct integer pointer arithmetic */
#define INT_SZ 4 /* integers are 32 bits or 4 bytes on a 32-bit system */

/* System indentifier. */
#define SYS_ERROR -1

/* The lock for file handling */
struct lock lock_fd;

/* Struct to hold data for the file descriptor list in thread */
struct fd_elem
{
    struct file * file_current;     /* a pointer to the current file */
    struct list_elem e;             /* list element to create list_fd entries */
    int fd;                         /* the actual file descriptor value */
};

void syscall_init (void);

/* User Implemented per 3.3.4 System Calls */
void halt (void);
void exit (int status);
pid_t exec (const char *cmd_line);
int wait (pid_t pid);
bool create (const char * file, unsigned initial_size);
bool remove (const char * file);
int open (const char * file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);
int symlink (char *target, char *linkpath);

/* User implemented functions */
struct child *get_child (pid_t pid, struct list *list_children);

#endif /* userprog/syscall.h */
