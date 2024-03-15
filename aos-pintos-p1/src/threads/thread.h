#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>

/* States in a thread's life cycle. */
enum thread_status
{
  THREAD_RUNNING, /* Running thread. */
  THREAD_READY,   /* Not running but ready to run. */
  THREAD_BLOCKED, /* Waiting for an event to trigger. */
  THREAD_DYING    /* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1) /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0      /* Lowest priority. */
#define PRI_DEFAULT 31 /* Default priority. */
#define PRI_MAX 63     /* Highest priority. */

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion.  (So don't add elements below
   THREAD_MAGIC.)
*/
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
{
  /* Owned by thread.c. */
  tid_t tid;                 /* Thread identifier. */
  enum thread_status status; /* Thread state. */
  char name[16];             /* Name (for debugging purposes). */
  uint8_t *stack;            /* Saved stack pointer. */
  int priority;              /* Priority. */
  struct list_elem allelem;  /* List element for all threads list. */

  /* Shared between thread.c and synch.c. */
  struct list_elem elem; /* List element. */

#ifdef USERPROG
  /* Owned by userprog/process.c. */
  uint32_t *pagedir; /* Page directory. */
#endif

/* Student implemented ----------------------------------------- */
/* Shared between thread.c and timer.c */
int64_t wake_up_ticks; /* wake up ticks member. */
struct list_elem blocked_elem; /* blocked list element. */

/* Shared between thread.c and synch.c. */
int true_priority; /* original priority during donation. */
struct list all_locks; /* all the locks held by the thread. */
struct lock *lock_current; /* Current lock on thread. */
/* End student implemented ------------------------------------- */

  /* Owned by thread.c. */
  unsigned magic; /* Detects stack overflow. */
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

/* Minor midification made by student. */
void thread_init (void);
void thread_start (void);

/* Significant modification made by student. */
void thread_tick (void);

void thread_print_stats (void);

/* Student implemented thread_cmp_priority. */
/* Shared between thread.c and timer.c.
   @brief - performs a comparison between two thread priorities.
   @param (const struct list_elem *) a - a pointer to a list element containing a thread.
   @param (const struct list_elem *) b - a pointer to a list element containing a thread.
   @param (void *) aux UNUSED - a pointer to a value unused (prototype similar to Project 0).
   @return (bool) - returns true if element b's priority is greater than or equal to element
                    a's priority, otherwise, returns false.
*/
bool thread_cmp_priority (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);

/* Minor modification made by student. */
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;

/* Minor modification made by student. */
void thread_yield (void);

/* Student implemented thread_try_yield. */
/* A pre-conditional check to aid thread_yield.
   @brief - The purpose of this function is to check if the current thread should yield the
            CPU to another thread that is ready to run and has higher priority. This occurs
            by comparing priorities between the current thread and the highest priority
            thread within the ready list.
   @param (void) - N/A
   @return (void) - N/A
*/
void thread_try_yield (void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void);

/* Modified by student. */
void thread_set_priority (int);

/* Shared between thread.c and synch.c. */
/* Student implemented thread_update_priority
   @brief - The purpose of this function is to update the effective priority of a given
            thread based upon its own true_priority member when compared with the highest
            priority of all threads within the all_locks list in which the current thread
            holds the lock for.
   @param (struct thread *) t - a pointer to the current thread whose priority requires updating
   @return (void) - N/A
*/
void thread_update_priority (struct thread *t);

/* Student implemented thread_resort_ready_list
   @brief - The purpose of this function is to update the position of a thread within
            the ready_list based upon the thread priority value, ensuring that the list
            remains sorted according to thread priority.
   @param (struct thread *) t - a pointer to the current thread to be repositioned within the
            ready_list.
   @return (void) - N/A
*/
void thread_resort_ready_list (struct thread *t);

/* Shared between thread.c and timer.c. */
/* Student implemented thread_set_blocked.
   @brief - The purpose of this function is to transition the current thread's status from
            THREAD_READY to THREAD_BLOCKED, moving the thread from a runnable status and
            placing the thread into a blocked state until certain conditions are met.
   @param (int64_t) ticks - the number of timer ticks indicating the moment a thread 
            should be considered for wake up from its blocked state.
   @return (void) - N/A
*/
void thread_set_blocked (int64_t ticks);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

#endif /* threads/thread.h */