#include "userprog/syscall.h"
#include <stdio.h>
#include <stdbool.h>
#include <syscall-nr.h>
#include <string.h>
#include "syscall.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include <user/syscall.h>
#include "devices/input.h"
#include "devices/shutdown.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"

#define ARG1      1
#define ARG2      2
#define ARG3      3
#define MAX_ARGS  3
#define WORD_SZ   4

#define PATH_MAX 256

typedef int pid_t;

static void syscall_handler (struct intr_frame *);
int file_add (struct file *file_name);
void get_args (struct intr_frame *f, int *argv, int argc);
void syscall_halt (void);
pid_t syscall_exec (const char *cmd_line);
int syscall_wait (pid_t pid);
bool syscall_create (const char *file, unsigned initial_size);
bool syscall_remove (const char *file);
int syscall_open (const char *file);
int syscall_filesize (int fd);
int syscall_read (int fd, void *buffer, unsigned size);
int syscall_write (int fd, const void *buffer, unsigned size);
void syscall_seek (int fd, unsigned position);
unsigned syscall_tell (int fd);
void syscall_close (int fd);
bool syscall_symlink (const char *target, const char *linkpath);
bool syscall_chdir (char *pathname);
bool syscall_mkdir (char *pathname);
bool syscall_readdir (int fd, char *pathname);
bool syscall_isdir (int fd);
int syscall_inumber (int fd);
int syscall_stat (const char *pathname, void *buf);
void validate_ptr (const void *vaddr);
void validate_str (const void *str);
void validate_buffer (const void *buf, unsigned size);

/*
 * System call initializer
 * It handles the set up for system call operations.
 */
void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

  /* initialize global file_system_lock */
  lock_init (&file_system_lock);
}

/*
 * This method handles for various case of system command.
 * This handler invokes the proper function call to be carried
 * out base on the command line.
 */
static void
syscall_handler (struct intr_frame *f) 
{
  int argv[MAX_ARGS] = { 0 };
  int esp = translate_vaddr ((const void *) f->esp);

  bool success    = false;
  char *target    = NULL;
  char *linkpath  = NULL;
  char *pathname  = NULL;
  
  switch (* (int *) esp)
  {
    case SYS_HALT:
      syscall_halt();
      break;
      
    case SYS_EXIT:
      /* Populate argv with required arguments */
      get_args (f, &argv[0], ARG1);
      syscall_exit (argv[0]);
      break;
      
    case SYS_EXEC:
      /* Populate argv with required arguments */
      get_args (f, &argv[0], ARG1);
      /* Check command line validity */
      validate_str ((const void *) argv[0]);
      /* Retrieve pointer to the page */
      argv[0] = translate_vaddr  ((const void *) argv[0]);
      /* set return value and execute command */
      f->eax = syscall_exec ((const void *) argv[0]);
      break;
      
    case SYS_WAIT:
      /* Populate argv with required arguments */
      get_args (f, &argv[0], ARG1);
      /* Set return value and execute command */
      f->eax = syscall_wait (argv[0]);
      break;
      
    case SYS_CREATE:
      /* Populate argv with required arguments */
      get_args (f, &argv[0], ARG2);
      /* Check command line validity */
      validate_str ((const void *) argv[0]);
      /* Retrieve pointer to the page */
      argv[0] = translate_vaddr  ((const void *) argv[0]);
      /* set return value and execute command */
      f->eax = syscall_create ((const char *) argv[0], (unsigned) argv[1]);
      break;
      
    case SYS_REMOVE:
      /* Populate argv with required arguments */
      get_args (f, &argv[0], ARG1);
      /* Check command line validity */
      validate_str ((const void*)argv[0]);
      /* Retrieve pointer to the page */
      argv[0] = translate_vaddr  ((const void *) argv[0]);
      /* Set return value and execute command */
      f->eax = syscall_remove ((const char *)argv[0]);
      break;
      
    case SYS_OPEN:
      /* Populate argv with required arguments */
      get_args (f, &argv[0], ARG1);
      /* Check command line validity */
       validate_str((const void*)argv[0]);
      /* Retrieve pointer to the page */
      argv[0] = translate_vaddr ((const void *)argv[0]);
      /* Set return value and execute command */
      f->eax = syscall_open((const char *)argv[0]);
      break;
      
    case SYS_FILESIZE:
      /* Populate argv with the required arguments */
      get_args (f, &argv[0], ARG1);
      /* Set return value and execute command */
      f->eax = syscall_filesize(argv[0]);
      break;
      
    case SYS_READ:
      /* Populate argv with required arguments */
      get_args (f, &argv[0], ARG3);
      /* Checl buffer validity */
      validate_buffer ((const void*) argv[1], (unsigned) argv[2]);
      /* Retrieve pointer to the page */
      argv[1] = translate_vaddr  ((const void *) argv[1]); 
      /* Set the return value and execute command */
      f->eax = syscall_read (argv[0], (void *) argv[1], (unsigned) argv[2]);
      break;
      
    case SYS_WRITE:
      /* Populate argv with required arguments */
      get_args (f, &argv[0], ARG3);
      /* Check buffer validity */
      validate_buffer ((const void*) argv[1], (unsigned) argv[2]);
      /* Retrieve pointer to the page */
      argv[1] = translate_vaddr  ((const void *) argv[1]); 
      /* Set the return value and execute command */
      f->eax = syscall_write (argv[0], (const void *) argv[1], (unsigned) argv[2]);
      break;
      
    case SYS_SEEK:
      /* Populate argv with required arguments */
      get_args (f, &argv[0], ARG2);
      /* Execute command */
      syscall_seek (argv[0], (unsigned)argv[1]);
      break;
      
    case SYS_TELL:
      /* Populate argv with required arguments */
      get_args (f, &argv[0], ARG1);
      /* Set return value and execute command */
      f->eax = syscall_tell (argv[0]);
      break;
    
    case SYS_CLOSE:
      /* Populate argv with required arguments */
      get_args (f, &argv[0], ARG1);
      /* execute command */
      syscall_close(argv[0]);
      break;
    
    case SYS_SYMLINK:
      /* Populate arguments from stack using WORD_SZ pointer arithmetic */
      target    = *(char **)((char*)f->esp + 4);
      linkpath  = *(char **)((char*)f->esp + 8);
      /* call symlink helper function to create symbolic link */
      success = syscall_symlink (target, linkpath);
      /* Set return value to success or -1 on failure*/
      f->eax = success ? 0 : -1;
      break;

    case SYS_CHDIR:
      pathname = *(char **)((char*)f->esp + 4);
      success = syscall_chdir (pathname);
      f->eax = success;
      break;

    case SYS_MKDIR:
      pathname = *(char **)((char*)f->esp + 4);
      success = syscall_mkdir (pathname);
      f->eax = success;
      break;

    case SYS_READDIR:
      /* Populate argv with required arguments */
      get_args (f, &argv[0], ARG2);
      /* Execute command */
      f->eax = syscall_readdir (argv[0], (char *)argv[1]);
      break;

    case SYS_ISDIR:
      get_args (f, &argv[0], ARG1);
      success = syscall_isdir (argv[0]);
      f->eax = success;
      break;

    case SYS_INUMBER:
      get_args (f, &argv[0], ARG1);
      f->eax = syscall_inumber (argv[0]);
      break;

    case SYS_STAT:
      get_args (f, &argv[0], ARG2);
      success = syscall_stat ((const char *) argv[0], (void *) argv[1]);
      f->eax = success ? 0 : -1;
      break;    
      
    default:
      f->eax = ERROR;
      break;
  }
}

/* Start User Implemented and Helper Functions */
/* get_args:
 * @brief - retrieves the appropriate amount of args from the stack frame.
 * @param (struct intr_frame *) f - a pointer to the current stack frame.
 * @param (int *) argv - a pointer to the memory locations of the arguments
 *                       on the stack frame.
 * @param (int) argc - the current number of arguments on the stack frame.
 * @return (void) - N/A
*/
void get_args (struct intr_frame *f, int *argv, int argc)
{
  int *ptr;
  for (int i = 0; i < argc; i++)
  {
    ptr = (int *) f->esp + i + 1;
    validate_ptr ((const void *) ptr);
    argv[i] = *ptr;
  }
}

/* halt:
 * @brief - calls shutdown_power_off (shutdown.h) to exit the current process abruptly.
 * @param (void) - N/A
 * @return (void) - N/A
 */
void syscall_halt (void)
{
  shutdown_power_off(); // from shutdown.h
}

/* syscall_exit:
 * @brief - Checks if the current thread to exit is a child. If it is,
 *          update the child's parent appropriately.
 * @param (int) status - the process exit status
 * @return (void) - N/A
 */
void syscall_exit (int status)
{
  struct thread *cur = thread_current ();
  struct list_elem *e;

  if ((thread_alive(cur->parent)) && (cur->child))
  {
    if (0 > status)
      status = -1;
    
    cur->child->status = status;
  }

  printf ("%s: exit(%d)\n", cur->name, status);

  while (!list_empty (&cur->file_list))
  {
    e = list_pop_front (&cur->file_list);
    struct process_file *pf = list_entry (e, struct process_file, elem);
    struct inode *inode = file_get_inode (pf->file);
    if ((NULL == pf) || (NULL == inode))
      continue;

    if (inode_is_dir (inode))
      dir_close ((struct dir *) pf->file);
    else
    {
      if (pf->file->deny_write)
        file_allow_write (pf->file);

      file_close (pf->file);
    }

    free (pf);
  }

  if (cur->dir)
  {
    dir_close (cur->dir);
    cur->dir = NULL;
  }

  thread_exit ();
}

/* syscall_exec
 * @brief - Executes the command line and returns the pid of the thread
 *          currently executing the command.
 * @param (const char *) cmd_line - a pointer to the command line buffer
 * @return (pid_t) - on success, returns the pid of the process executed
 *                   otherwise returns ERROR (-1).
 */
pid_t syscall_exec (const char* cmd_line)
{
    pid_t pid = process_execute (cmd_line);
    struct child_process *child_process_ptr = child_process_find (pid);
    if (!child_process_ptr)
    {
      return ERROR;
    }
    /* check if process if loaded */
    if (NOT_LOADED == child_process_ptr->load_status)
    {
      sema_down (&child_process_ptr->load_sema);
    }
    /* check if process failed to load */
    if (LOAD_FAIL == child_process_ptr->load_status)
    {
      child_process_remove (child_process_ptr);
      return ERROR;
    }
    return pid;
}

/* syscall_wait:
 * @brief - calls process_wait (process.h), to wait on a process to execute.
 * @param (pid_t) pid - the process ID to wait on.
 * @return (int) - returns the value received from process_wait().
 */
int syscall_wait (pid_t pid)
{
  return process_wait (pid);
}

/* syscall_create:
 * @brief - calls filesys_create (filesys.h) to create a file of the appropriate
 *          name and size.
 * @param (const char *) file - a pointer to the name of the file to be created.
 * @param (unsigned) initial_size - the size of the file upon creation.
 * @return (bool) - returns the value received from filesys_create().
 */
bool syscall_create (const char *file, unsigned initial_size)
{
  lock_acquire (&file_system_lock);
  bool successful = filesys_create (file, initial_size, false); // from filesys.h
  lock_release (&file_system_lock);
  return successful;
}

/* syscall_remove:
 * @brief - calls filesys_remove (filesys.h) to remove the provided file.
 * @param (const char *) file - a pointer to the name of the file to remove.
 * @return (bool) - returns the value received from filesys_remove().
 */
bool syscall_remove (const char* file)
{
  lock_acquire (&file_system_lock);
  bool successful = filesys_remove (file); // from filesys.h
  lock_release (&file_system_lock);
  return successful;
}

/* syscall_open:
 * @brief - calls filesys_open (filesys.h) to open the provided file by name
 * @param (const char *) file - a pointer to the name of the file to open.
 * @return (int) - returns the appropriate file descriptor given to the open
 *                 file on success, or ERROR (-1) on failure.
 */
int syscall_open (const char *file)
{
  lock_acquire (&file_system_lock);
  struct file *file_ptr = filesys_open (file); // from filesys.h
  if (!file_ptr)
  {
    lock_release (&file_system_lock);
    return ERROR;
  }
  int fd = file_add (file_ptr);
  lock_release (&file_system_lock);
  return fd;
}

/* syscall_filesize:
 * @brief - returns the size of the provided file (in bytes)
 * @param (int) fd - the file descriptor for the file to retrieve the size.
 * @return (int) - on success, returns an integer value of the size of the
 *                 file, or ERROR (-1) on failure.
 */
int syscall_filesize (int fd)
{
  lock_acquire (&file_system_lock);
  struct file *file_ptr = file_get (fd);
  if (!file_ptr)
  {
    lock_release (&file_system_lock);
    return ERROR;
  }

  if (inode_is_dir (file_get_inode (file_ptr)))
  {
    lock_release (&file_system_lock);
    return ERROR;
  }

  int fsize = file_length (file_ptr); // from file.h
  lock_release (&file_system_lock);
  return fsize;
}

/* syscall_read:
 * @brief - determines the appropriate INPUT method and attempts to read the file
 *          provided by the file descriptor, placing its contents into the buffer.
 *          If the INPUT device is a file, an attempt to call file_read (file.h)
 *          will be made to read the file. If the INPUT device is deemed "user",
 *          an attempt to read from input_getc (input.h) will be made.
 * @param (int) fd - the file descriptor of the file to read.
 * @param (void *) buffer - the container to store the contents of the file.
 * @param (unsigned) size - the amount of bytes to read from the INPUT device.
 * @return (int) - on success, returns an integer value of the amount of bytes
 *                 read from the INPUT device, or ERROR (-1) on failure.
 */
int syscall_read (int fd, void *buffer, unsigned size)
{
  lock_acquire (&file_system_lock);
  if (0 >= size)
  {
    lock_release (&file_system_lock);
    return size;
  }
  
  if (STD_INPUT == fd)
  {
    uint8_t *buffer_copy = (uint8_t *) buffer;
    for (unsigned i = 0; i < size; i++)
      /* retrieve pressed key from the input buffer */
      buffer_copy[i] = input_getc (); // from input.h
    
    lock_release (&file_system_lock);
    return size;
  }
  
  /* read from file */
  struct file *file_ptr = file_get (fd);
  if (!file_ptr)
  {
    lock_release (&file_system_lock);
    return ERROR;
  }

  struct inode *inode = file_get_inode (file_ptr);
  if (NULL == inode)
  {
    lock_release (&file_system_lock);
    return ERROR;
  }

  if (inode_is_dir (inode))
  {
    lock_release (&file_system_lock);
    return ERROR;
  }

  int bytes_read = file_read (file_ptr, buffer, size); // from file.h
  lock_release (&file_system_lock);
  return bytes_read;
}

/* syscall_write:
 * @brief - determines the appropriate OUTPUT method and attempts to write to the file
 *          provided by the file descriptor, retrieving its contents from the buffer.
 *          If the OUTPUT device is a file, an attempt to call file_write (file.h)
 *          will be made to read the file. If the OUTPUT device is deemed "user",
 *          an attempt to write to putbuf (stdio.h) will be made.
 * @param (int) fd - the file descriptor of the file to write.
 * @param (void *) buffer - the container to retrieve the contents of the file.
 * @param (unsigned) size - the amount of bytes to write to the OUTPUT device.
 * @return (int) - on success, returns an integer value of the amount of bytes
 *                 written to the OUTPUT device, or ERROR (-1) on failure.
 */
int syscall_write (int fd, const void * buffer, unsigned size)
{
  lock_acquire (&file_system_lock);
  if (0 >= size)
  {
    lock_release (&file_system_lock);
    return size;
  }
  if (STD_OUTPUT == fd)
  {
    putbuf (buffer, size); // from stdio.h
    lock_release (&file_system_lock);
    return size;
  }
  
  // start writing to file
  struct file *file_ptr = file_get (fd);
  if (!file_ptr)
  {
    lock_release (&file_system_lock);
    return ERROR;
  }

  struct inode *inode = file_get_inode (file_ptr);
  if (NULL == inode)
  {
    lock_release (&file_system_lock);
    return ERROR;
  }

  if (inode_is_dir (inode))
  {
    lock_release (&file_system_lock);
    return ERROR;
  }

  int bytes_written = file_write (file_ptr, buffer, size); // file.h
  lock_release (&file_system_lock);
  return bytes_written;
}

/* syscall_seek:
 * @brief - moves the file cursor to the desired position based upon the value passed.
 * @param (int) fd - the file descriptor of the file.
 * @param (unsigned) - the desired position within the file to seek to.
 * @return (void) - N/A
 */
void syscall_seek (int fd, unsigned position)
{
  lock_acquire(&file_system_lock);
  struct file *file_ptr = file_get (fd);
  if (!file_ptr)
  {
    lock_release (&file_system_lock);
    return;
  }

  if (inode_is_dir (file_get_inode (file_ptr)))
  {
    lock_release (&file_system_lock);
    return;
  }

  file_seek (file_ptr, position);
  lock_release (&file_system_lock);
}

/* syscall_tell:
 * @brief - determines the current file position offset by calling file_tell (file.h).
 * @param (int) fd - the file descriptor of the file to determine the offset.
 * @return (unsigned) - returns the retrieved value of file_tell() or ERROR (-1) on failure.
 */
unsigned syscall_tell (int fd)
{
  lock_acquire (&file_system_lock);
  struct file *file_ptr = file_get (fd);
  if (!file_ptr)
  {
    lock_release (&file_system_lock);
    return ERROR;
  }

  if (inode_is_dir (file_get_inode (file_ptr)))
  {
    lock_release (&file_system_lock);
    return ERROR;
  }

  off_t offset = file_tell (file_ptr); //from file.h
  lock_release (&file_system_lock);
  return offset;
}

/* syscall_close:
 * @brief - calls the helper function process_file_close (syscall.h) to close the current file.
 * @param (int) fd - the file descriptor of the file to close.
 * @return (void) - N/A
 */
void syscall_close (int fd)
{
  lock_acquire (&file_system_lock);
  process_file_close (fd);
  lock_release (&file_system_lock);
}

/* syscall_symlink:
 * @brief - determined validity of parameters passed and calls filesys_open/filesys_symlink
 *          (filesys.h) to open and create the symbolic link and file_close (file.h) to
 *          close the target file after the link was created
 * @param (const char *) target - a pointer to the target link path.
 * @param (const char *) linkpath - a pointer to the path created by the symbolic link.
 * @return (bool) - returns the value retrieved from filesys_symlink (filesys.h) or false
 *                  on failure.
 */
bool syscall_symlink (const char *target, const char *linkpath)
{
  /* pointer validity check */
  if ((!is_user_vaddr (target)) || (!is_user_vaddr (linkpath)))
    return false;

  if ((NULL == target) || (NULL == linkpath))
    return false;

  struct file *target_file = filesys_open ((char *) target);
  if (NULL == target_file)
    return false;

  file_close (target_file);
  bool success = filesys_symlink ((char *) target, (char *) linkpath); // filesys.h
  return success;
}

bool syscall_chdir (char *pathname)
{
  if (!is_user_vaddr (pathname))
    return false;

  bool success = filesys_chdir(pathname);
  return success;

  return true;
}

bool syscall_mkdir (char *pathname)
{
  if (!is_user_vaddr (pathname))
    return false;

  bool success = filesys_create (pathname, 0, true);
  return success;
}

bool syscall_readdir (int fd, char *pathname)
{
  if (!is_user_vaddr (pathname))
    return false;

  struct file *file = file_get (fd);
  if (NULL == file)
    return false;

  struct inode *inode = file_get_inode (file);
  if (NULL == inode)
    return false;

  if (!inode_is_dir (inode))
    return false;

  struct dir *dir = (struct dir *) file;
  if (!dir_readdir (dir, pathname))
    return false;

  return true;
}

bool syscall_isdir (int fd)
{
  struct file *file = file_get (fd);
  if (NULL == file)
    return false;

  struct inode *inode = file_get_inode (file);
  if (NULL == inode)
    return false;

  if (!inode_is_dir (inode))
    return false;

  return true;
}

int syscall_inumber (int fd)
{
  struct file *file = file_get (fd);
  if (NULL == file)
    return ERROR;

  struct inode *inode = file_get_inode (file);
  if (NULL == inode)
    return ERROR;

  block_sector_t inumber = inode_get_inumber (inode);
  return inumber;
}

int syscall_stat (const char *pathname, void *buf)
{
  if ((!is_user_vaddr (pathname)) || (!is_user_vaddr (buf)))
    return -1;

  struct stat st;
  int result = filesys_stat (pathname, &st);
  if (result)
    memcpy (buf, &st, sizeof (struct stat));
  
  // printf ("value of result :%d\n", result);
  return result;
}

/* validate_ptr:
 * @brief - a function to check if pointer is valid based upon the known user virtual memory space.
 * @param (const void *) vaddr - the address of the item to determine is validity.
 * @return (void) - N/A
 */
void validate_ptr (const void *vaddr)
{
  if ((USER_VADDR_BOTTOM > vaddr) || (!is_user_vaddr (vaddr)))
    // virtual memory address is not reserved for us (out of bound)
    syscall_exit (ERROR);

  // Cast the void pointer to a char pointer for the arithmetic operation
  char *char_vaddr = (char *) vaddr;

  // Check the next 3 bytes
  for (int i = 0; i < WORD_SZ; i++) 
  {
    void *addr = char_vaddr + i;
    if ((!is_user_vaddr (addr)) || (NULL == pagedir_get_page (thread_current ()->pagedir, addr))) 
      syscall_exit (ERROR);
  }
}

/* valdiate_str:
 * @brief - this function is meant to check if string is valid via its page address.
 * @param (const void *) str - a pointer to the string that requires validation.
 * @return (void) - N/A
 */
void validate_str (const void *str)
{
    while (0 != *(char *) translate_vaddr (str))
      str = (char *) str + 1;
}

/* validate_buffer:
 * @brief - similar to valdate_string, this function checks if buffer is valid via its
 *          virtual page address, and determined if its size does not leak into 
 *          unauthorized space.
 * @param (const void *) buf - a pointer to the buffer that requires validation.
 * @param (unsigned) size - the size of the buffer being checked. */
void validate_buffer (const void *buf, unsigned size)
{
  unsigned i = 0;
  char* buffer_copy = (char *) buf;
  while (i < size)
  {
    validate_ptr ((const void *) buffer_copy);
    buffer_copy++;
    i++;
  }
}

/* translate_vaddr:
 * @brief - get the pointer to page and translate the virutal address to its physical address.
 * @param (const void *) vaddr - a pointer to the specific virtual address.
 * @return (int) - returns an integer representation of the associated physical address, or 
 *                 ERROR (-1) on failure.
 */
int translate_vaddr (const void *vaddr)
{
  if (!is_user_vaddr (vaddr))
    syscall_exit (ERROR);

  void *ptr = pagedir_get_page (thread_current ()->pagedir, vaddr);
  if (!ptr)
  {
    syscall_exit (ERROR);
  }
  return (int) ptr;
}

/* child_process_find:
 * @brief - find a child process based on a specific pid from the child process list struct.
 * @param (int) pid - the pid of the specific child process to be found.
 * @return (struct child_process *) - on success returns a pointer to the child process struct
 *                                    associated with the specific PID, if not found, returns NULL.
 */
struct child_process *child_process_find (int pid)
{
  struct thread *t = thread_current ();
  struct list_elem *e;
  struct list_elem *next;
  
  for (e = list_begin (&t->child_list); list_end (&t->child_list) != e; e = next)
  {
    next = list_next (e);
    struct child_process *child = list_entry (e, struct child_process, elem);
    if (pid == child->pid)
    {
      return child;
    }
  }
  return NULL;
}

/* child_process_remove:
 * @brief - removes a specific child process based upon the child struct passed.
 * @param (struct child_process *) child - a pointer to the specific child process to remove.
 * @return (void) - N/A
 */
void child_process_remove  (struct child_process *child)
{
  thread_lock_release ();
  list_remove (&child->elem);
  free (child);
}

/* child_process_remove_all:
 * @brief - remove all child processes from a thread containing the parent process.
 * @param (void) - N/A
 * @return (void) - N/A
 */
void child_process_remove_all (void) 
{
  struct thread *t = thread_current ();
  struct list_elem *next;
  struct list_elem *e = list_begin (&t->child_list);
  
  while (list_end (&t->child_list) != e)
  {
    next = list_next (e);
    struct child_process *child = list_entry (e, struct child_process, elem);
    if (NULL == child)
      return;

    child_process_remove (child);
    e = next;
  }
}

/* file_add:
 * @brief - add file to file list and return file descriptor of added file.
 * @param (struct file *) file_name - a pointer to the file struct (file.h) containing
 *                                    the name of the specific file.
 * @return (int) - on success, returns the file descriptor of the newly created
 *                 file, or ERROR (-1) on failure.
 */
int file_add (struct file *file_name)
{
  struct process_file *process_file_ptr = malloc (sizeof (struct process_file));
  if (!process_file_ptr)
  {
    file_close (file_name);
    return ERROR;
  }
  process_file_ptr->file  = file_name;
  process_file_ptr->fd    = thread_current ()->fd;
  thread_current ()->fd++;
  list_push_back (&thread_current ()->file_list, &process_file_ptr->elem);
  return process_file_ptr->fd;
  
}

/* file_get:
 * @brief - get file that matches the specific file descriptor passed.
 * @param (int) fd - the file descriptor of the specific file to locate.
 * @return (struct file *) - returns a pointer to the specific file struct
 *                           (file.h) of the associated file descriptor.
 */
struct file *file_get (int fd)
{
  struct thread *t = thread_current ();
  struct list_elem* next;
  struct list_elem* e = list_begin (&t->file_list);
  if (NULL == e)
  {
   process_file_close (fd);
    return NULL;
  }
  
  while (list_end (&t->file_list) != e)
  {
    struct process_file *process_file_ptr = list_entry (e, struct process_file, elem);
    next = list_next (e); // Prepare 'next' for the next iteration

    if (process_file_ptr == NULL)
    {
        file_close(process_file_ptr->file);
        return NULL;
    }

    if (fd == process_file_ptr->fd)
        return process_file_ptr->file;

    e = next;
  }

  /* return NULL if process file not found */
  return NULL;
}

/* process_file_close:
 * @brief - close the desired file descriptor of the process file (syscall.h).
 * @param (int) fd - the file descriptor of the specified file to close.
 * @return (void) - N/A
 */
void process_file_close (int fd)
{
  struct thread *t = thread_current ();
  struct list_elem *next;
  struct list_elem *e = list_begin (&t->file_list);
  
  while (list_end (&t->file_list) != e)
  {
    next = list_next (e);
    struct process_file *process_file_ptr = list_entry (e, struct process_file, elem);
    if (NULL == process_file_ptr)
      /* list_entry does not find process_file */
      break;

    struct inode *inode = file_get_inode (process_file_ptr->file);
    if (NULL == inode)
      break;

    if ((fd == process_file_ptr->fd) || (FD_CLOSE_ALL == fd))
    {
      if (inode_is_dir (inode))
        dir_close ((struct dir *) process_file_ptr->file);
      else
        file_close (process_file_ptr->file);
      
      list_remove (&process_file_ptr->elem);
      free (process_file_ptr);

      if (FD_CLOSE_ALL != fd)
        return;
    }

    e = next;
  }
}