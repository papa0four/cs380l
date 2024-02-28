#ifndef THREADS_SYNCH_H
#define THREADS_SYNCH_H

#include <list.h>
#include <stdbool.h>
#include <debug.h>

/* A counting semaphore. */
struct semaphore
{
  unsigned value;      /* Current value. */
  struct list waiters; /* List of waiting threads. */
};

void sema_init (struct semaphore *, unsigned value);
void sema_down (struct semaphore *);
bool sema_try_down (struct semaphore *);
void sema_up (struct semaphore *);
void sema_self_test (void);

/* Used to retrieve the thread with the max priority. */
struct thread * sema_max_priority (struct semaphore *);

/* Lock. */
struct lock
{
  struct thread *holder;      /* Thread holding lock. */
  struct semaphore semaphore; /* Binary semaphore controlling access. */

  /* Shared between synch.c and thread.c. */
  struct list_elem lock_elem; /* stored in all locks in THREAD struct. */
  int max_priority; /* highest priority of all locked threads within current lock. */
};

void lock_init (struct lock *);
void lock_acquire (struct lock *);
bool lock_try_acquire (struct lock *);
void lock_release (struct lock *);
bool lock_held_by_current_thread (const struct lock *);

/* Update the priority of a lock in the waiters list. */
void lock_update_priority (struct lock *t_lock);

/* Condition variable. */
struct condition
{
  struct list waiters; /* List of waiting threads. */
};

void cond_init (struct condition *);
void cond_wait (struct condition *, struct lock *);
void cond_signal (struct condition *, struct lock *);
void cond_broadcast (struct condition *, struct lock *);

/* Shared between synch.c and thread.c. */
bool lock_cmp_priority (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);
bool sema_cmp_priority (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);

/* Optimization barrier.

   The compiler will not reorder operations across an
   optimization barrier.  See "Optimization Barriers" in the
   reference guide for more information.*/
#define barrier() asm volatile("" : : : "memory")

#endif /* threads/synch.h */