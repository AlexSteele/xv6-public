
#include "types.h"
#include "user.h"
#include "fcntl.h"

char buf[1024];

void
check(int cond, char *s)
{
  if (!cond) {
    printf(2, "error: %s\n", s);
    exit();
  }
}

void
genericwrites(char *fname, int nblocks, int wsize)
{
  int fd;
  int i;
  int start;
  int end;
  
  printf(1, "nblocks=%d\n", nblocks);
  printf(1, "wsize=%d\n", wsize);

  fd = open(fname, O_CREATE|O_RDWR);
  check(fd > 0, "open");
  memset(buf, 1, sizeof(buf));
  start = uptime();
  for (i = 0; i < nblocks; i++) {
    int j;
    for (j = 0; j < 512/wsize; j++) {
      int n = 0;
      int nw;
      while (n < wsize) {
        check((nw = write(fd, buf + n, wsize - n)) > 0, "write");
        n += nw;
      }      
    }
  }
  end = uptime();
  printf(1, "elapsed ticks: %d\n", end - start);

  check(close(fd) == 0, "close");
}

void
blockwrites(void)
{
  printf(1, "blockwrites test\n");
  genericwrites("blockwrites.txt", 140, 512);
  printf(1, "ok\n");
}

void
smallwrites(void)
{
  printf(1, "smallwrites test\n");
  genericwrites("smallwrites.txt", 32, 16);
  printf(1, "ok\n");
}

int
main(void)
{
  printf(1, "fstest\n");

  blockwrites();
  smallwrites();

  printf(1, "success\n");
  exit();
}
