
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

  printf(1, "argtest\n");

//  addr = mmap(1, 0, 4096);
//  check(addr == 0, "mapped stdin");

  addr = mmap(3, 0, 4096);
  check(addr == 0, "mapped bad descriptor");

  printf(1, "ok\n");
}

void
basicrw(void)
{
  char *msg = "abcdefghijklmnopqrstuvwxyz";
  char *msg2 = "zyxwvutsrqponmlkjihgfedcba";
  int len = strlen(msg)+1;
  int niter = 256;
  int fd;
  int i;
  char *addr;

  printf(1, "basicrw test\n");

  fd = open(fname, O_CREATE|O_RDWR);
  check(fd > 0, "open");

  for (i = 0; i < niter; i++) {
    check(write(fd, msg, len) == len, "write");
  }

  close(fd);

  // Check reads match the file
  fd = open(fname, O_RDWR);
  check(fd > 0, "open (2)");

  addr = mmap(fd, 0, 8192);
  check(addr > 0, "mmap");

  // Should be able to close the file
  close(fd);

  for (i = 0; i < niter; i++) {
    char buf[128];
    memmove(buf, addr + (i * len), len);
    check(strcmp(msg, buf) == 0, "strcmp (1)");
  }

  // Check writes go to disk
  for (i = 0; i < niter; i++) {
    memmove(addr + (i * len), msg2, len);
  } 

  check(munmap(addr) == 0, "munmap");

  fd = open(fname, O_RDONLY);
  check(fd > 0, "open (3)");

  for (i = 0; i < niter; i++) {
    char buf[128];
    check(read(fd, buf, len) == len, "read");
    check(strcmp(msg2, buf) == 0, "strcmp (2)"); 
  }

  close(fd);

  printf(1, "ok\n");
}

int
main(void)
{
  printf(1, "mmap test\n");

  argtest();
  basicrw();

  printf(1, "success\n"); 
  exit();
}
