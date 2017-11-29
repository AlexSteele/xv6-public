
#include "fcntl.h"
#include "types.h"
#include "user.h"

char *test_file = "testfile.txt";

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
  int rc;

  printf(1, "argtest\n");

  addr = mmap(1, 0, 4096);
  check(addr == 0, "mapped stdin");

  addr = mmap(3, 0, 4096);
  check(addr == 0, "mapped bad descriptor");

  rc = munmap((void *) (1 << 23));
  check(rc, "munmap non-mapped region");

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

  fd = open(test_file, O_CREATE|O_RDWR);
  check(fd > 0, "open");

  for (i = 0; i < niter; i++) {
    check(write(fd, msg, len) == len, "write");
  }

  close(fd);

  // Check reads match the file
  fd = open(test_file, O_RDWR);
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

  fd = open(test_file, O_RDONLY);
  check(fd > 0, "open (3)");

  for (i = 0; i < niter; i++) {
    char buf[128];
    check(read(fd, buf, len) == len, "read");
    check(strcmp(msg2, buf) == 0, "strcmp (2)");
  }

  close(fd);

  printf(1, "ok\n");
}

// Unmapping a region should make its pages unaccessible.
void
unmap(void)
{

  printf(1, "unmap\n");

  if (fork() == 0) {
    int fd;
    char *addr;

    fd = open(test_file, O_CREATE|O_RDWR);
    check(fd > 0, "open");

    addr = mmap(fd, 0, 4096);
    check(addr > 0, "mmap");

    check(munmap(addr) == 0, "munmap");

    // This should cause a page fault
    printf(1, "should see page fault\n");
    addr[0] = 'a';

    check(0, "able to access unmapped region");
  }
  wait();
  printf(1, "ok\n");
}

// A file unlinked while it is mmapped should not
// be removed from disk until after it is unmapped.
void
unlinktest(void)
{
  char *addr;
  int fd;

  printf(1, "unlinktest\n");

  fd = open(test_file, O_CREATE|O_RDWR);
  check(fd > 0, "open");

  addr = mmap(fd, 0, 4096);
  check(addr > 0, "mmap");

  check(close(fd) == 0, "close");
  check(unlink(test_file) == 0, "unlink");

  memmove(addr, "abcdefg", strlen("abcdefg"));

  // Moment of truth. Unmapping file should write
  // contents to disk without issue despite unlink().
  check(munmap(addr) == 0, "munmap");

  // Now file should be gone.
  check(open(test_file, O_RDONLY) < 0, "open after mmap");

  printf(1, "ok\n");
}

void
forktest(void)
{
  char *msg = "12345678";
  int len = strlen(msg);
  char *addr;
  int fd;
  int pid;
  int rc;
  int i;

  printf(1, "forktest\n");

  fd = open(test_file, O_CREATE|O_RDWR);
  check(fd, "open");

  addr = mmap(fd, 0, 4096);
  check(addr > 0, "mmap failed");

  // Write initial bytes from parent proc
  memmove(addr, msg, len);

  pid = fork();
  check(pid >= 0, "fork failed");
  if (pid == 0) {
    char buf[128];

    close(fd);

    // Check bytes written from parent are present.
    memmove(buf, addr, len);
    buf[len] = 0;
    check(strcmp(buf, msg) == 0, "strcmp from child");

    // Write to region from child
    for (i = 0; i < 512; i++) {
      memmove(addr + (i * 8), msg, len);
    }

    // Unmap region
    rc = munmap(addr);
    check(rc == 0, "child munmap");
    exit();
  }
  wait();

  for (i = 0; i < 512; i++) {
    char buf[128];
    memmove(buf, addr + (i * 8), len);
    buf[len] = 0;
    check(strcmp(msg, buf) == 0, "strcmp from parent");
  }

  rc = munmap(addr);
  check(rc == 0, "parent munmap");

  printf(1, "ok\n");
}

// Different processes should be able to mmap overlapping
// regions of a file.
void
regionoverlap(void)
{
  char *msg = "abcdefghijklmnopqrstuv";
  int len = strlen(msg);
  char *addr;
  int fd;
  int rc;
  int i;

  printf(1, "regionoverlap\n");

  fd = open(test_file, O_CREATE|O_RDWR);
  check(fd > 0, "open");

  if (fork() == 0) {

    addr = mmap(fd, 4096, 4096);
    check(addr > 0, "mmap from child");

    for (i = 0; (i * len) < 4096 - len; i++) {
      memmove(addr + (i * len), msg, len);
    }

    check(munmap(addr) == 0, "munmap from child");
    close(fd);
    exit();
  }
  wait();

  // Parent's region overlaps but does not equal child's region
  addr = mmap(fd, 0, 8192);
  check(addr > 0, "mmap from parent");

  // Parent and child should be able to touch the
  // non-overlapping portions of memory at the same time
  for (i = 0; (i * len) < 4096 - len; i++) {
    memmove(addr + (i * len), msg, len);
  }

  // Wait for child to unmap the region.
  wait();

  // Parent should still be able to  touch all portions
  // without a page fault.
  memmove(addr, msg, len);

  // Check changes by child are viewable by parent
  for (i = 0; (i * len) < 4096 - len; i++) {
    char buf[128];
    memmove(buf, addr + 4096 + (i * len), len);
    buf[len] = 0;
    check(strcmp(buf, msg) == 0, "strcmp in parent failed");
  }

  rc = munmap(addr);
  check(rc == 0, "munmap from parent");
  close(fd);

  printf(1, "ok\n");
}

int
main(void)
{
  printf(1, "mmap test\n");

  argtest();
  basicrw();
  unmap();
  unlinktest();
  forktest();
  regionoverlap();

  printf(1, "success\n");
  exit();
}
