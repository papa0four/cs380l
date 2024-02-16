/* The main thread downs a semaphore.  Then it creates a
   higher-priority thread that blocks being able to down the semaphore
   again, causing it to donate its priority to the main thread.  The main
   thread attempts to lower its priority, which should not take
   effect until the donation is released. */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/synch.h"
#include "threads/thread.h"

static thread_func down_thread_func;

void test_priority_donate_lower_sema (void)
{
  struct semaphore sema;

  /* Make sure our priority is the default. */
  ASSERT (thread_get_priority () == PRI_DEFAULT);

  sema_init (&sema, 1);
  sema_down (&sema);
  thread_create ("down", PRI_DEFAULT + 10, down_thread_func, &sema);
  msg ("Main thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT + 10, thread_get_priority ());

  msg ("Lowering base priority...");
  thread_set_priority (PRI_DEFAULT - 10);
  msg ("Main thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT + 10, thread_get_priority ());
  sema_up (&sema);
  msg ("down must already have finished.");
  msg ("Main thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT - 10, thread_get_priority ());
}

static void down_thread_func (void *sema_)
{
  struct semaphore *sema = sema_;

  sema_down (sema);
  msg ("down: downed the semaphore");
  sema_up (sema);
  msg ("down: done");
}
