/* Makes change to target file and reads from symbolic link. */

#include <syscall.h>
#include "tests/userprog/sample.inc"
#include "tests/lib.h"
#include "tests/main.h"

void test_main (void)
{
  int target_fd, link_fd;
  char *target = "test.txt";
  char *linkpath = "test-link.txt";

  CHECK (create ("test.txt", sizeof sample - 1), "create \"test.txt\"");
  CHECK ((target_fd = open ("test.txt")) > 1, "open \"test.txt\"");

  int result = symlink (target, linkpath);
  if (result != 0)
    {
      fail ("symlink() returned %d instead of 0", result);
    }
  else
    {
      msg ("symlink() successfully returned 0");
    }

  CHECK ((link_fd = open ("test-link.txt")) > 1, "open \"test-link.txt\"");

  char content[] = "This is a test";
  CHECK (write (target_fd, content, sizeof content) == sizeof content,
         "write content to \"test.txt\"");
  char buf[sizeof content];
  CHECK (read (link_fd, buf, sizeof content) == sizeof content,
         "read \"test-link.txt\"");

  msg ("test-link.txt reads: '%s'", buf);
}