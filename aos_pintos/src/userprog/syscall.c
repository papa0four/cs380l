#include "userprog/syscall.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "devices/block.h"
#include "threads/interrupt.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "devices/shutdown.h"
#include "filesys/file.h"
#include "filesys/filesys.h"

#define check_null(PTR) \
  do { \
    if ((!is_user_vaddr(PTR)) || \
        (!pagedir_get_page(thread_current()->pagedir, PTR))) \
      exit(SYS_ERROR); \
  } while (0)

static void syscall_1 (struct intr_frame *f, int syscall, void *args);
static void syscall_2 (struct intr_frame *f, int syscall, void *args);
// static void syscall_3 (struct intr_frame *f, int syscall, void *args);
static void syscall_handler (struct intr_frame *);

void syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler (struct intr_frame *f)
{
  int syscall_num = *((intptr_t *) f->esp);
  void *args = f->esp;

  switch (syscall_num)
  {
    case SYS_HALT:
      halt ();
      break;

    case SYS_EXIT:
      syscall_1 (f, SYS_EXIT, args);
      break;

    case SYS_EXEC:
      syscall_1 (f, SYS_EXEC, args);
      break;

    case SYS_WAIT:
      syscall_1 (f, SYS_WAIT, args);
      break;

    case SYS_CREATE:
      syscall_2 (f, SYS_CREATE, args);
      break;

    case SYS_REMOVE:
      syscall_1 (f, SYS_REMOVE, args);
      break;

    case SYS_OPEN:
      syscall_1 (f, SYS_OPEN, args);
      break;

  //   case SYS_FILESIZE:
  //     // implement to handle filesize syscall
  //     break;

  //   case SYS_READ:
  //     // implement to handle read syscall
  //     break;

  //   case SYS_WRITE:
  //     // implement to handle write syscall
  //     break;

  //   case SYS_SEEK:
  //     // implement to handle seek syscall
  //     break;

  //   case SYS_TELL:
  //     // implement to handle tell syscall
  //     break;

  //   case SYS_CLOSE:
  //     // implement to handle close syscall
  //     break;

  //   case SYS_SYMLINK:
  //     // implement to handle symlink syscall
  //     break;

    default:
      thread_exit();
      break;
  }
  // printf ("system call!\n");
  thread_exit ();
}

static void syscall_1 (struct intr_frame *f, int syscall, void *args)
{
  int arg = *((int *) args);

  switch (syscall)
  {
    case SYS_EXIT:
      exit (arg);
      break;

    case SYS_EXEC:
      check_null ((const char *) arg);
      f->eax = exec ((const char *) arg);
      break;

    case SYS_WAIT:
      f->eax = wait (arg);
      break;

    case SYS_REMOVE:
      check_null ((const void *) arg);
      f->eax = remove ((const char *) arg);
      break;

    case SYS_OPEN:
      check_null ((const void *) arg);
      f->eax = open ((const char *) arg);
      break;

    case SYS_FILESIZE:
      f->eax = filesize (arg);
      break;

    case SYS_TELL:
      f->eax = tell (arg);
      break;

    case SYS_CLOSE:
      close (arg);
      break;

    default:
      break;
  }
}

static void syscall_2 (struct intr_frame *f, int syscall, void *args)
{
  int arg0 = *((int *) args);
  args = (char *)args + INT_SZ;
  int arg1 = *((int *) args);

  switch (syscall)
  {
    case SYS_CREATE:
      check_null ((const void *) arg0);
      f->eax = create ((const char *) arg0, (unsigned) arg1);
      break;

    case SYS_SEEK:
      seek (arg0, (unsigned) arg1);
      break;

    case SYS_SYMLINK:
      check_null ((char *) arg0);
      f->eax = symlink ((char *) arg0, (char *) arg1);
      break;

    default:
      /* set eax to error value */
      f->eax = SYS_ERROR;
      break;
  }
}

// static void syscall_3 (struct intr_frame *f, int syscall, void *args)
// {
//   int *argv = (int *) args;
//   int arg0 = argv[0];
//   void *arg1 = (void *)argv[1];
//   unsigned arg2 = (unsigned) argv[2];

//   check_null (arg1);
//   check_null (((char *) arg1 + arg2) - 1); // check the last byte accessed

//   if (SYS_WRITE == syscall)
//     f->eax = write (arg0, arg1, arg2);
//   else
//     f->eax = read (arg0, arg1, arg2);
// }

void halt (void) { shutdown_power_off (); }

void exit (int status)
{
  struct thread *cur = thread_current ();
  printf ("thread (%s) exited with status: %d\n", cur->name, status);

  /* get child process */
  struct child * child_process = get_child(cur->tid, &cur->parent->list_children);
  child_process->exit_status = status;

  /* set current status for child process */
  if (PID_ERROR == status)
    child_process->status = CHILD_KILLED;
  else
    child_process->status = CHILD_EXITED;
  
  thread_exit ();
}

pid_t exec (const char * cmd_line)
{
  if (NULL == cmd_line)
    return PID_ERROR;

  struct thread *cur  = thread_current ();
  tid_t         pid   = -1;

  pid = process_execute (cmd_line);

  struct child *child_process = get_child ((pid_t) pid, &cur->list_children);
  sema_down (&child_process->child->sema_exec);
  if (!child_process->load_status)
    return PID_ERROR;
  return (pid_t) pid;
}

int wait (pid_t pid) { return process_wait ((tid_t) pid); }

bool create (const char *file, unsigned initial_size)
{
  /* lock critical section */
  lock_acquire (&lock_fd);
  bool ret_val = filesys_create (file, initial_size);
  lock_release (&lock_fd);
  return ret_val;
}

bool remove (const char * file)
{
  /* lock critical section */
  lock_acquire (&lock_fd);
  bool ret_val = filesys_remove (file);
  lock_release (&lock_fd);
  return ret_val;
}

int open (const char * file)
{
  /* lock critical section */
  lock_acquire (&lock_fd);
  struct file *fd_open = filesys_open (file);
  lock_release (&lock_fd);

  if (NULL == fd_open)
    return SYS_ERROR;

  /* track the file descriptor within the process */
  struct thread *cur = thread_current ();
  /* increment and assign fd */
  int fd = ++cur->file_sz;

  /* allocate memory for file descriptor element */
  struct fd_elem *file_elem = malloc (sizeof (struct fd_elem));
  if (NULL == file_elem)
  {
    file_close (fd_open);
    return SYS_ERROR;
  }

  /* set member of the fd_elem */
  file_elem->fd = fd;
  file_elem->file_current = fd_open;

  /* add file_elem to the list_fd list of the current process */
  list_push_back (&cur->list_fd, &file_elem->e);

  return fd;
}

/* get_child:
* @brief - iterates over the list of child processes looking for the
           matching pid_t.
* @param (pid_t) pid - the pid to match with the current process.
* @param (struct list *) list_children - the list structure containing
         the child processes.
  @return - if the matching pid is found, the pointer to the child process
            is returned, other wise NULL is returned if it is not found.
*/
struct child *get_child (pid_t pid, struct list *list_children)
{
  struct list_elem *e = NULL;
  for (e = list_begin (list_children); list_end (list_children) != e; e = list_next (e))
  {
    struct child * child_process = list_entry (e, struct child, child_elem);
    if (pid == child_process->pid)
    {
      return child_process;
    }
  }

  return NULL;
}