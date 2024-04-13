#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "syscall.h"
#include "vm/frame.h"
#include "vm/page.h"

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy;
  tid_t tid;
  struct thread *child = NULL;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);

  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (file_name, PRI_DEFAULT, start_process, fn_copy);

  if (tid == TID_ERROR)
    palloc_free_page (fn_copy); 

  if(tid != TID_ERROR)
    child = get_thread_by_tid (tid);
  // add a new 'child' struct to parent's children list
  if (child != NULL) {
    list_push_back (&thread_current()->children, &child->child_elem);
    sema_down (&child->load_sema);
    if(child->load_success == -1) {
      tid = TID_ERROR;
    }
  }

  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  char *file_name = file_name_;
  struct intr_frame if_;
  struct thread *t = thread_current ();
  bool success;

  /* hash_init calls malloc, which throws an error if done
     within thread_init. As far as I'm aware, this is the
     earliest point at which hash_init can be called */
  hash_init (&t->pages, spt_hash_func, spt_less_func, NULL);

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (file_name, &if_.eip, &if_.esp);

  /* If load failed, quit. */
  palloc_free_page (file_name);
  if (!success) {
    t->load_success = -1;
    sema_up (&t->load_sema);
    thread_exit ();
  }
  sema_up (&t->load_sema);

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid UNUSED) 
{
  int exit_status;
  struct thread *t = thread_current ();
  struct thread *child = NULL;
  struct list_elem *e;

  /* Get child whose tid is tid if one exists */
  for (e = list_begin (&t->children); e != list_end (&t->children);
       e = list_next (e))
    {
      child = list_entry (e, struct thread, child_elem);
      if(child->tid == child_tid)
        break;
    }
  if (e == list_end (&t->children))
    return -1;
  list_remove (&child->child_elem);

  sema_down (&child->wait_sema);

  exit_status = child->exit_status;

  sema_up (&child->exit_sema);

  // printf ("EXIT_STATUS: %d\n", exit_status);
  return exit_status;
}

/* Free the current process's resources. */
void
process_exit (void)
{

  ASSERT (NULL != (void *) &thread_current ()->executables);
  ASSERT (NULL != (void *) &thread_current ()->pages);

  struct thread *cur = thread_current ();
  struct thread *child = NULL;
  struct list_elem *e;
  struct list_elem *prv;
  uint32_t *pd;

  hash_destroy (&cur->pages, page_destructor);

  /* Close all opened files and free their pages */
  for (e = list_begin (&cur->executables); e != list_end (&cur->executables);)
  {
    prv = e;
    e = list_next (e);
    struct openfile *of = list_entry (prv, struct openfile, elem);
    if (NULL != of)
    {
      file_close (of->file);
      list_remove (&of->elem);
      palloc_free_page (of);
    }
  }

  if(cur->command != NULL)
    printf("%s: exit(%d)\n", cur->command, cur->exit_status);

  sema_up (&cur->wait_sema);

  for (e = list_begin (&cur->children); e != list_end (&cur->children);
       e = list_next (e))
  {
    child = list_entry (e, struct thread, child_elem);
    sema_down (&child->wait_sema);
    sema_up (&child->exit_sema);
  }


  // wait for parent to reap
  sema_down (&cur->exit_sema);

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  palloc_free_page (cur->command);
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp, const char *file_name);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  char *token, *save_ptr, *fn_copy;
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  /* Open executable file, extracting only the first token (the filename) */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);

  token = strtok_r (fn_copy, " ", &save_ptr);

  file = filesys_open (token);
  struct openfile *new = palloc_get_page (0);
  new->fd = thread_current ()->next_fd;
  thread_current ()->next_fd++;
  new->file = file;
  //file = filesys_open (file_name);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }
  list_push_back(&thread_current ()->executables, &new->elem);
  file_deny_write (file);

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp, file_name))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  struct thread *t = thread_current ();

  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      struct spt_entry *p = (struct spt_entry *) malloc (sizeof (struct spt_entry));
      p->addr = upage;
      if (p == NULL)
      {
        free (p);
        return false;
      }
      p->is_stack_page = false;
      p->frame = NULL;
      p->file = file;
      p->offset = ofs;
      p->status = FILE_MAPPED;
      p->writable = writable;
      p->read_bytes = page_read_bytes;
      p->pagedir = t->pagedir;
      p->sector = SECTOR_INIT;

      hash_insert (&t->pages, &p->hash_elem);

      /* Advance. */
      read_bytes  -= page_read_bytes;
      zero_bytes  -= page_zero_bytes;
      ofs         += page_read_bytes;
      upage       += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp, const char *file_name) 
{
  struct thread *t = thread_current ();
  uint8_t *kpage;
  bool success = false;

  char *token;
  char *save_ptr;
  char *fn_copy;
  char **tokens = palloc_get_page (0);
  int i = 0;
  int len_token = 0;
  char *esp_char;
  uint8_t word_align = 0;

  fn_copy = palloc_get_page (0);
  if (NULL == fn_copy)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);

  for (token = strtok_r (fn_copy, " ", &save_ptr); token != NULL;
        token = strtok_r (NULL, " ", &save_ptr))
  {
    if(0 == i)
    {
      thread_current ()->command = palloc_get_page (0);
      strlcpy(thread_current ()->command, token, strlen (token) + 1);
    }
    tokens[i] = token;
    i++;
  }

  /* create a page, put it in a frame, then set stack */
  struct spt_entry *p = (struct spt_entry *) malloc (sizeof (struct spt_entry));
  p->is_stack_page = true;
  p->addr = (void *)((char *) PHYS_BASE - PGSIZE);
  p->status = FRAME_MAPPED;
  p->writable = true;
  p->file = NULL;
  p->offset = 0;
  p->read_bytes = 0;
  p->pagedir = thread_current ()->pagedir;
  p->sector = SECTOR_INIT;
  hash_insert (&t->pages, &p->hash_elem);
  t->page_cnt++;

  struct frame_entry *stack_frame = frame_get ();
  p->frame = stack_frame;
  kpage = stack_frame->kpage;

  stack_frame->page = p;
  if (kpage != NULL) 
  {
    success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
    if (success)
    {
      *esp = PHYS_BASE;
      esp_char = (char *) *esp;

      /* push arguments onto the stack from right to left */
      int j;
      int len;
      for (j = i; j > 0; j--)
      {
        len = strlen(tokens[j - 1]);
        len_token += len + 1;
        esp_char -= len + 1;
        strlcpy(esp_char, tokens[j - 1], len + 1);
        tokens[j - 1] = esp_char;
      }

      /* word_align */
      len_token = WORDSZ - (len_token % WORDSZ);
      for (j = 0; j < len_token; j++)
      {
        esp_char--;
        *esp_char = word_align;
      }

      /* add zero char pointer */
      esp_char -= WORDSZ;
      *esp_char = 0;

      /* add pointers to arguments on stack */
      for (j = i; j > 0; j--)
      {
        esp_char -= WORDSZ;
        *((int *) esp_char) = (unsigned) tokens[j - 1];
      }

      /* put argv onto the stack */
      void *tmp = esp_char;
      esp_char -= WORDSZ;
      *((int *) esp_char) = (unsigned) tmp;

      /* put argc onto the stack */
      esp_char -= WORDSZ;
      *esp_char = i;
      esp_char -= WORDSZ;
      *esp_char = 0;

      /* move esp to bottom of stack */
      *esp = esp_char;

      //hex_dump(*esp, *esp, PHYS_BASE-*esp, 1);
    }
    else
      palloc_free_page (kpage);
  }

  palloc_free_page (fn_copy);
  palloc_free_page (tokens);
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return ((NULL == pagedir_get_page (t->pagedir, upage)) &&
          (pagedir_set_page (t->pagedir, upage, kpage, writable)));
}