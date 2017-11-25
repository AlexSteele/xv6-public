
#include "fcntl.h"
#include "types.h"
#include "user.h"

char *fname = "sample.txt";

void
check(int cond, char *msg)
{
  if (!cond) {
    printf(2, "error: %s\n", msg);
    exit();
  }
}

void
argtest(void)
{
  char *addr;
  int fd;

  printf(1, "argtest\n");

  addr = mmap(1, 0, 4096);
  check(addr == 0, "mapped stdin");

  addr = mmap(3, 0, 4096);
  check(addr == 0, "mapped bad descriptor");

  fd = open(fname, O_CREATE|O_RDWR);
  check(fd > 0, "open");

  addr = mmap(fd, -1, 4096);
  check(addr == 0, "mapped neg offset");

  addr = mmap(fd, 0, -1);
  check(addr == 0, "mapped negative len");

  close(fd);
  printf(1, "ok\n");
}

int
main(void)
{
  printf(1, "mmap test\n");

  argtest();

  printf(1, "success\n"); 
  exit();
}
