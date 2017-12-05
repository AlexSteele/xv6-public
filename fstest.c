
#include "types.h"
#include "user.h"
#include "fcntl.h"

char buf[1024];
char buf2[1024];

void
check(int cond, char *s)
{
  if (!cond) {
    printf(2, "error: %s\n", s);
    exit();
  }
}

int
strncmp(char *s1, char *s2, int n)
{
  while (n && *s1 && *s1 == *s2)
    s1++, s2++, n--;
  return n == 0 ? 0 : (uchar)*s1 - (uchar)*s2;
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
  printf(1, "blockwrites bench\n");
  genericwrites("blockwrites.txt", 140, 512);
  check(unlink("blockwrites.txt") == 0, "blockwrites");
  printf(1, "ok\n");
}

void
smallwrites(void)
{
  printf(1, "smallwrites bench\n");
  genericwrites("smallwrites.txt", 32, 16);
  check(unlink("smallwrites.txt") == 0, "unlink");
  printf(1, "ok\n");
}

void
basicrw(void)
{
  int fd;
  int i;

  printf(1, "basicrw test\n");

  for (i = 0; i < sizeof(buf); i++) {
    buf[i] = i%64;
  }

  check((fd = open("basicrw.txt", O_CREATE|O_RDWR)) > 0, "open");
  for (i = 1; i <= 1024; i*=2) {
    int n = 0;
    int nw;
    while (n < i) {
      check((nw = write(fd, buf + n, i - n)) > 0, "write");
      n += nw;
    }
  }
  check(close(fd) == 0, "close");
  check((fd = open("basicrw.txt", O_RDONLY)) > 0, "open (2)");
  for (i = 1; i <= 1024; i*=2) {
    int n = 0;
    int nr;
    while (n < i) {
      check((nr = read(fd, buf2 + n, i - n)) > 0, "read");
      n += nr;
    }
    check(strncmp(buf, buf2, i) == 0, "strcmp failed\n");
  }
  check(close(fd) == 0, "close");
  check(unlink("basicrw.txt") == 0, "unlink");

  printf(1, "ok\n");
}

void
dirtest(void)
{
  int fd;
  int i;

  printf(1, "dirtest\n");

  check(mkdir("testdir") == 0, "mkdir");

  for (i = 0; i < 20; i++) {
    char name[] = "testdir/t0";
    name[strlen(name)-1] = (char) i;
    check((fd = open(name, O_CREATE|O_RDWR)) > 0, "open");
    check(write(fd, name, strlen(name)) == strlen(name), "write");
    check(close(fd) == 0, "close");
  }

  // Check that we can re-open files.
  for (i = 0; i < 20; i++) {
    char name[] = "testdir/t0";
    name[strlen(name)-1] = (char) i;
    check((fd = open(name, O_RDONLY)) > 0, "open");
    check(close(fd) == 0, "close");
    check(unlink(name) == 0, "unlink");
  }

  check(unlink("testdir") == 0, "unlink");

  printf(1, "ok\n");
}

// Writes should be reliable even under heavy file system activity.
void
hotfiles(void)
{
  int nprocs = 8;
  int pid;
  int i;
  int fds[10];

  printf(1, "hotfiles test\n");

  memset(buf, 'a', sizeof(buf));

  for (i = 0; i < sizeof(fds)/sizeof(int); i++) {
    char name[] = "hotx";
    name[strlen(name)-1] = i;
    fds[i] = open(name, O_CREATE|O_RDWR);
    check(fds[i] > 0, "open");
  }

  for (i = 0; i < nprocs; i++) {
    check((pid = fork()) >= 0, "fork");
    if (pid == 0) {
      break;
    }
  }
  if (pid == 0) {
    char name[] = "px";
    int fd;
    name[strlen(name)-1] = (char) getpid();
    memset(buf, (char) getpid(), sizeof(buf));
    for (i = 0; i < sizeof(fds)/sizeof(int); i++) {
      check(write(fds[i], buf, 32) > 0, "write");
    }
    // Create process-local file.
    check((fd = open(name, O_CREATE|O_RDWR)) > 0, "open");
    for (i = 0; i < sizeof(fds)/sizeof(int); i++) {
      check(write(fds[i], buf, 32) > 0, "write");
    }
    // Write to it.
    for (i = 0; i < 4; i++) {
      check(write(fd, buf, sizeof(buf)) == sizeof(buf), "write");
    }
    // Close it.
    check(close(fd) == 0, "close");
    for (i = 0; i < sizeof(fds)/sizeof(int); i++) {
      check(write(fds[i], buf, 32) > 0, "write");
    }
    // Open it.
    check((fd = open(name, O_RDONLY)) > 0, "open");
    for (i = 0; i < sizeof(fds)/sizeof(int); i++) {
      check(write(fds[i], buf, 32) > 0, "write");
    }
    // Check that reads are accurate.
    for (i = 0; i < 4; i++) {
      check(read(fd, buf2, sizeof(buf2)) == sizeof(buf2), "read");
      check(strncmp(buf, buf2, sizeof(buf)) == 0, "strncmp failed");
    }
    check(close(fd) == 0, "close");
    check(unlink(name) == 0, "unlink");
    exit();
  }
  while (wait() >= 0)
    ;
  for (i = 0; i < sizeof(fds)/sizeof(int); i++) {
    char name[] = "hotx";
    name[strlen(name)-1] = i;
    check(close(fds[i]) == 0, "close");
    check(unlink(name) == 0, "unlink");
  }

  printf(1, "ok\n");
}

int
main(void)
{
  printf(1, "fstest\n");

  basicrw();
  dirtest();
  hotfiles();
  blockwrites();
  /* smallwrites(); */

  printf(1, "success\n");
  exit();
}
