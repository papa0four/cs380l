/* The main thread creates and downs a binary semaphore.  Then it creates two
   higher-priority threads that block being able to down the semaphore again,
   causing them to donate their priorities to the main thread.  When the
   main thread ups the semaphore, the other threads should
   down it in priority order.
 */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/synch.h"
#include "threads/thread.h"

static thread_func down1_thread_func;
static thread_func down2_thread_func;

void test_priority_donate_one_sema (void)
{
  struct semaphore sema;

  /* Make sure our priority is the default. */
  ASSERT (thread_get_priority () == PRI_DEFAULT);

  sema_init (&sema, 1);
  sema_down (&sema);
  thread_create ("down1", PRI_DEFAULT + 1, down1_thread_func, &sema);
  msg ("This thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT + 1, thread_get_priority ());
  thread_create ("down2", PRI_DEFAULT + 2, down2_thread_func, &sema);
  msg ("This thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT + 2, thread_get_priority ());
  sema_up (&sema);
  msg ("down2, down1 must already have finished, in that order.");
  msg ("This should be the last line before finishing this test.");
}

static void down1_thread_func (void *sema_)
{
  struct semaphore *sema = sema_;

  sema_down (sema);
  msg ("down1: downed the semaphore");
  sema_up (sema);
  msg ("down1: done");
}

static void down2_thread_func (void *sema_)
{
  struct semaphore *sema = sema_;

  sema_down (sema);
  msg ("down2: downed the semaphore");
  sema_up (sema);
  msg ("down2: done");
}
