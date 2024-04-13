#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "vm/page.h"


#define BUF_MAX 200

static bool valid_mem_access (const void *);
static void syscall_handler (struct intr_frame *);
static void userprog_halt (void);
static void userprog_exit (int);
static pid_t userprog_exec (const char *);
static int userprog_wait (pid_t);
static bool userprog_create (const char *, unsigned);
static bool userprog_remove (const char *);
static int userprog_open (const char *);
static int userprog_filesize (int);
static int userprog_read (int, void *, unsigned);
static int userprog_write (int, const void *, unsigned);
static void userprog_seek (int, unsigned);
static unsigned userprog_tell (int);
static void userprog_close (int);
static bool syscall_symlink (const char *target, const char *linkpath);
static struct openfile * file_get (int);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&filesys_lock);
}

static bool
valid_mem_access (const void *up)
{
	// struct thread *t = thread_current ();

	if (up == NULL)
		return false;
  if (is_kernel_vaddr (up))
    return false;
  
	return true;
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  void *esp = f->esp;
  uint32_t *eax = &f->eax;

  if(!valid_mem_access ( ((int *) esp)))
    userprog_exit (-1);
  if(!valid_mem_access ( ((int *) esp) + 1))
    userprog_exit (-1);

  char *target    = NULL;
  char *linkpath  = NULL;

  switch (* (int *) esp)
  {
  	case SYS_HALT:
  	  userprog_halt ();
  	  break;

  	case SYS_EXIT:
    {
  	  int status = *(((int *) esp) + 1);
  	  userprog_exit (status);
  	  break;
    }

  	case SYS_EXEC:
    {
  	  const char *cmd_line = *(((char **) esp) + 1);
  	  *eax = (uint32_t) userprog_exec (cmd_line);
  	  break;
    }

  	case SYS_WAIT:
    {
  	  pid_t pid = *(((pid_t *) esp) + 1);
  	  *eax = (uint32_t) userprog_wait (pid);
  	  break;
    }

  	case SYS_CREATE:
    {
  	  const char *file = *(((char **) esp) + 1);
  	  unsigned initial_size = *(((unsigned *) esp) + 2);
  	  *eax = (uint32_t) userprog_create (file, initial_size);
  	  break;
    }

  	case SYS_REMOVE:
    {
  	  const char *file = *(((char **) esp) + 1);
  	  *eax = (uint32_t) userprog_remove (file);
  	  break;
    }

  	case SYS_OPEN:
    {
  	  const char *file = *(((char **) esp) + 1);
  	  *eax = (uint32_t) userprog_open (file);
  	  break;
    }

  	case SYS_FILESIZE:
    {
  	  int fd = *(((int *) esp) + 1);
  	  *eax = (uint32_t) userprog_filesize (fd);
  	  break;
    }

  	case SYS_READ:
    {
  	  int fd = *(((int *) esp) + 1);
  	  void *buffer = (void *) *(((int **) esp) + 2);
  	  unsigned size = *(((unsigned *) esp) + 3);
  	  *eax = (uint32_t) userprog_read (fd, buffer, size);
  	  break;
    }

  	case SYS_WRITE:
    {
  	  int fd = *(((int *) esp) + 1);
  	  const void *buffer = (void *) *(((int **) esp) + 2);
  	  unsigned size = *(((unsigned *) esp) + 3);
  	  *eax = (uint32_t) userprog_write (fd, buffer, size);
  	  break;
    }

  	case SYS_SEEK:
    {
  	  int fd = *(((int *) esp) + 1);
  	  unsigned position = *(((unsigned *) esp) + 2);
  	  userprog_seek (fd, position);
  	  break;
    }

  	case SYS_TELL:
    {
  	  int fd = *(((int *) esp) + 1);
  	  *eax = (uint32_t) userprog_tell (fd);
  	  break;
    }

  	case SYS_CLOSE:
    {
  	  int fd = *(((int *) esp) + 1);
  	  userprog_close (fd);
  	  break;
    }

    case SYS_SYMLINK:
    {
      target    = *(char **)((char *) f->esp + 4);
      linkpath  = *(char **)((char *) f->esp + 8);
      bool success = syscall_symlink (target, linkpath);
      *eax = success ? 0 : -1;
      break;
    }

    default:
      *eax = -1;
      break;
  }
}

static void
userprog_halt ()
{
	shutdown_power_off ();
}

static void
userprog_exit (int status)
{
  struct thread *cur = thread_current ();
  cur->exit_status = status;
	thread_exit ();
}

static pid_t
userprog_exec (const char *cmd_line)
{
  tid_t child_tid = TID_ERROR;

  if(!valid_mem_access (cmd_line))
    userprog_exit (-1);

  child_tid = process_execute (cmd_line);

	return child_tid;
}

static int
userprog_wait (pid_t pid)
{
  return process_wait (pid);
}

static bool
userprog_create (const char *file, unsigned initial_size)
{
  bool retval;
  if(valid_mem_access(file))
  {
    lock_acquire (&filesys_lock);
    retval = filesys_create (file, initial_size);
    lock_release (&filesys_lock);
    return retval;
  }
	else
    userprog_exit (-1);

  return false;
}

static bool
userprog_remove (const char *file)
{
  bool retval;
	if(valid_mem_access(file))
  {
    lock_acquire (&filesys_lock);
    retval = filesys_remove (file);
    lock_release (&filesys_lock);
    return retval;
  }
  else
    userprog_exit (-1);

  return false;
}

static int
userprog_open (const char *file)
{
	if(valid_mem_access ((void *) file))
  {
    struct openfile *new = palloc_get_page (0);
    new->fd = thread_current ()->next_fd;
    thread_current ()->next_fd++;

    lock_acquire (&filesys_lock);
    new->file = filesys_open(file);
    lock_release (&filesys_lock);

    if (new->file == NULL)
      return -1;
    list_push_back(&thread_current ()->executables, &new->elem);
    return new->fd;
  }
	else
    userprog_exit (-1);

	return -1;

}

static int
userprog_filesize (int fd)
{
  int retval;
  struct openfile *of = NULL;
	of = file_get (fd);
  if (of == NULL)
    return 0;

  lock_acquire (&filesys_lock);
  retval = file_length (of->file);
  lock_release (&filesys_lock);

  return retval;
}

static int
userprog_read (int fd, void *buffer, unsigned size)
{
  lock_acquire (&filesys_lock);
  int bytes_read = 0;
  struct openfile *of = NULL;

	if (!valid_mem_access(buffer))
  {
    lock_release (&filesys_lock);
    userprog_exit (-1);
  }

  // bufChar = (char *)buffer;
	if(0 == fd)
  {
    while(size > 0)
    {
      input_getc();
      size--;
      bytes_read++;
    }
    lock_release (&filesys_lock);
  }
  else
  {
    of = file_get (fd);
    if (of == NULL)
    {
      lock_release (&filesys_lock);
      return -1;
    }
    
    bytes_read = file_read (of->file, buffer, size);
    lock_release (&filesys_lock);
  }

  return bytes_read;
}

static int
userprog_write (int fd, const void *buffer, unsigned size)
{
  lock_acquire (&filesys_lock);
  int bytes_written = 0;
  char *bufChar = NULL;
  struct openfile *of = NULL;

	if (!valid_mem_access(buffer))
  {
    lock_release (&filesys_lock); 
		userprog_exit (-1);
  }
  bufChar = (char *)buffer;
  if(1 == fd)
  {
    /* break up large buffers */
    while(size > BUF_MAX)
    {
      putbuf(bufChar, BUF_MAX);
      bufChar += BUF_MAX;
      size -= BUF_MAX;
      bytes_written += BUF_MAX;
    }

    putbuf(bufChar, size);
    bytes_written += size;
    lock_release (&filesys_lock);
  }
  else
  {
    of = file_get (fd);
    if (of == NULL)
    {
      lock_release (&filesys_lock);
      return 0;
    }

    bytes_written = file_write (of->file, buffer, size);
    lock_release (&filesys_lock);
  }

  return bytes_written;
}

static void
userprog_seek (int fd, unsigned position)
{
	struct openfile *of = NULL;
  of = file_get (fd);
  if (of == NULL)
    return;

  lock_acquire (&filesys_lock);
  file_seek (of->file, position);
  lock_release (&filesys_lock);
}

static unsigned
userprog_tell (int fd)
{
  unsigned retval;
	struct openfile *of = NULL;
  of = file_get (fd);
  if (of == NULL)
    return 0;

  lock_acquire (&filesys_lock);
  retval = file_tell (of->file);
  lock_release (&filesys_lock);

  return retval;
}

static void
userprog_close (int fd)
{
	struct openfile *of = NULL;
  of = file_get (fd);
  if (of == NULL)
    return;

  lock_acquire (&filesys_lock);
  file_close (of->file);
  lock_release (&filesys_lock);

  list_remove (&of->elem);
  palloc_free_page (of);
}

static bool syscall_symlink (const char *target, const char * linkpath)
{
  if ((!is_user_vaddr (target)) || (!is_user_vaddr (linkpath)))
    return false;

  if ((NULL == target) || (NULL == linkpath))
    return false;

  struct file *target_file = filesys_open ((char *) target);
  if (NULL == target_file)
    return false;

  file_close (target_file);
  bool success = filesys_symlink ((char *) target, (char *) linkpath);
  return success;
}

static struct openfile *
file_get (int fd)
{
  struct thread *t = thread_current ();
  struct list_elem *e;
  for (e = list_begin (&t->executables); e != list_end (&t->executables);
       e = list_next (e))
    {
      struct openfile *of = list_entry (e, struct openfile, elem);
      if(of->fd == fd)
        return of;
    }
    
  return NULL;
}