/* Low-priority main thread L downs semaphore A.  Medium-priority
   thread M then downs semaphore B, but is blocked from being able to down 
   semaphore A again. High-priority thread H is blocked from being able
   to down semaphore B again.  Thus, thread H donates its priority to M,
   which in turn donates it to thread L.
 */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/synch.h"
#include "threads/thread.h"

struct semas
{
  struct semaphore *a;
  struct semaphore *b;
};

static thread_func medium_thread_func;
static thread_func high_thread_func;

void test_priority_donate_nest_sema (void)
{
  struct semaphore a, b;
  struct semas semas;

  /* Make sure our priority is the default. */
  ASSERT (thread_get_priority () == PRI_DEFAULT);

  sema_init (&a, 1);
  sema_init (&b, 1);

  sema_down (&a);

  semas.a = &a;
  semas.b = &b;
  thread_create ("medium", PRI_DEFAULT + 1, medium_thread_func, &semas);
  thread_yield ();
  msg ("Low thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT + 1, thread_get_priority ());

  thread_create ("high", PRI_DEFAULT + 2, high_thread_func, &b);
  thread_yield ();
  msg ("Low thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT + 2, thread_get_priority ());

  sema_up (&a);
  thread_yield ();
  msg ("Medium thread should just have finished.");
  msg ("Low thread should have priority %d.  Actual priority: %d.", PRI_DEFAULT,
       thread_get_priority ());
}

static void medium_thread_func (void *semas_)
{
  struct semas *semas = semas_;

  sema_down (semas->b);
  sema_down (semas->a);

  msg ("Medium thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT + 2, thread_get_priority ());
  msg ("Medium thread downed the semaphores.");

  sema_up (semas->a);
  thread_yield ();

  sema_up (semas->b);
  thread_yield ();

  msg ("High thread should have just finished.");
  msg ("Middle thread finished.");
}

static void high_thread_func (void *sema_)
{
  struct semaphore *sema = sema_;

  sema_down (sema);
  msg ("High thread downed the semaphore.");
  sema_up (sema);
  msg ("High thread finished.");
}
