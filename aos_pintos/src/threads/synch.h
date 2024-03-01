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

/* Minor modification made by student. */
void sema_down (struct semaphore *);

bool sema_try_down (struct semaphore *);

/* Major functionality change made by student. */
void sema_up (struct semaphore *);

void sema_self_test (void);

/* Student implemented sema_max_priority. */
/* 
  @brief - The purpose of this function is to find and return the thread with
           the highest priority from the sema->waiters list waiting for a 
           specific semaphore.
  @param (struct semaphore *) - a pointer to the semaphore structure whose waiters
          waiters list contains currently blocked threads waiting for a signal.
  @return (struct thread *) - returns a pointer to the thread who has the highest
          priority within the waiters list.
*/
struct thread * sema_max_priority (struct semaphore *);

/* Lock. */
struct lock
{
  struct thread *holder;      /* Thread holding lock. */
  struct semaphore semaphore; /* Binary semaphore controlling access. */

  /* Student implemented lock struct members. */
  /* Shared between synch.c and thread.c. */
  struct list_elem lock_elem; /* stored in all locks in THREAD struct. */
  int max_priority; /* highest priority of all locked threads within current lock. */
};

void lock_init (struct lock *);

/* Significant modification made by student. */
void lock_acquire (struct lock *);

bool lock_try_acquire (struct lock *);

/* Minor modification made by student. */
void lock_release (struct lock *);

bool lock_held_by_current_thread (const struct lock *);

/* Student implemented lock_update_priority. */
/*
  @brief - The purpose of this function is to update the maximum priority associated with
           a lock based on the priority values of the threads waiting on the lock's 
           semaphore to aid in priority donation.
  @param (struct lock *) t_lock - a pointer to the lock structure whose maximum priority
           requires updating.
  @return (void) - N/A
*/
void lock_update_priority (struct lock *t_lock);

/* Condition variable. */
struct condition
{
  struct list waiters; /* List of waiting threads. */
};

void cond_init (struct condition *);

/* Minor student modifications made. */
void cond_wait (struct condition *, struct lock *);

/* Minor student modification made. */
void cond_signal (struct condition *, struct lock *);
void cond_broadcast (struct condition *, struct lock *);

/* Student implemented lock_cmp_priority & semp_cmp_priority. */
/* Shared between synch.c and thread.c. */
/*
   @brief - performs a comparison between two thread priorities within the locked list.
   @param (const struct list_elem *) a - a pointer to a list element containing a locked thread.
   @param (const struct list_elem *) b - a pointer to a list element containing a locked thread.
   @param (void *) aux UNUSED - a pointer to a value unused (prototype similar to Project 0).
   @return (bool) - returns true if element b's priority is greater than or equal to element
                    a's priority, otherwise, returns false.
*/
bool lock_cmp_priority (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);

/*
   @brief - performs a comparison between two thread priorities within the semaphore waiters list.
   @param (const struct list_elem *) a - a pointer to a list element containing a thread containing the max priority.
   @param (const struct list_elem *) b - a pointer to a list element containing a thread containing the max priority.
   @param (void *) aux UNUSED - a pointer to a value unused (prototype similar to Project 0).
   @return (bool) - returns true if element b's priority is greater than or equal to element
                    a's priority, otherwise, returns false.
*/
bool sema_cmp_priority (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);

/* Optimization barrier.

   The compiler will not reorder operations across an
   optimization barrier.  See "Optimization Barriers" in the
   reference guide for more information.*/
#define barrier() asm volatile("" : : : "memory")

#endif /* threads/synch.h */