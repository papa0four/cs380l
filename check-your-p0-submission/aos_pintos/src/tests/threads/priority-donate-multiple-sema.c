/* The main thread downs semaphores A and B, then it creates two
   higher-priority threads.  Each of these threads blocks
   being able to down one of the semaphores again and thus donate
   their priority to the main thread.  The main thread ups the semaphores
   in turn and relinquishes its donated priorities.
*/

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/synch.h"
#include "threads/thread.h"

static thread_func a_thread_func;
static thread_func b_thread_func;

void test_priority_donate_multiple_sema (void)
{
  struct semaphore a, b;

  /* Make sure our priority is the default. */
  ASSERT (thread_get_priority () == PRI_DEFAULT);

  sema_init (&a, 1);
  sema_init (&b, 1);

  sema_down (&a);
  sema_down (&b);

  thread_create ("a", PRI_DEFAULT + 1, a_thread_func, &a);
  msg ("Main thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT + 1, thread_get_priority ());

  thread_create ("b", PRI_DEFAULT + 2, b_thread_func, &b);
  msg ("Main thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT + 2, thread_get_priority ());

  sema_up (&b);
  msg ("Thread b should have just finished.");
  msg ("Main thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT + 1, thread_get_priority ());

  sema_up (&a);
  msg ("Thread a should have just finished.");
  msg ("Main thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT, thread_get_priority ());
}

static void a_thread_func (void *sema_)
{
  struct semaphore *sema = sema_;

  sema_down (sema);
  msg ("Thread a downed semaphore a.");
  sema_up (sema);
  msg ("Thread a finished.");
}

static void b_thread_func (void *sema_)
{
  struct semaphore *sema = sema_;

  sema_down (sema);
  msg ("Thread b downed semaphore b.");
  sema_up (sema);
  msg ("Thread b finished.");
}
