
#include "types.h"
#include "user.h"
#include "fcntl.h"

char buf[64] = {0};
void noop(void) {}

void
check(int cond, char *s)
{
  if (!cond) {
    printf(2, "error: %s\n", s);
    exit();
  }
}

// Child should be able to read parent's mem.
void
forkread(void)
{
  int a = 3;
  int npages = 20;
  int pid;
  char *mem;
  int i;

  printf(1, "forkread test\n");

  mem = sbrk(npages * 4096);
  check(mem != (char*)-1, "sbrk (1)");
  memset(mem, 0, npages * 4096);

  pid = fork();
  check(pid >= 0, "fork");

  if (pid == 0) {
    check(a == 3, "a (child)");
    check(buf[0] == 0, "buf (child)");
    noop();
    for (i = 0; i < npages*4096; i++) {
      check(mem[i] == 0, "mem (child)");
    }
    check(sbrk(-(npages * 4096)) != (char*)-1, "sbrk (child)");
    exit();
  }
  check(wait() >= 0, "wait");

  // Parent should still be able to access everything.
  check(a == 3, "a (parent)");
  check(buf[0] == 0, "buf (parent)");
  noop();
  for (i = 0; i < npages*4096; i++) {
      check(mem[i] == 0, "mem (parent)");
  }
  check(sbrk(-(npages * 4096)) != (char*)-1, "sbrk (parent)");

  printf(1, "ok\n");
}

// Writes by child shouldn't be visible by parent.
void
forkwrite1(void)
{
  int a = 3;
  int npages = 20;
  int pid;
  char *mem;
  int i;

  printf(1, "forkwrite1 test\n");

  check((mem = sbrk(npages * 4096)) != (char*)-1, "sbrk");
  memset(mem, 0, npages * 4096);
  check((pid = fork()) >= 0, "fork");

  if (pid == 0) {
    a = 4;
    buf[0] = 'a';
    memset(mem, 1, npages * 4096);
    check(sbrk(-(npages *4096)) != (char*)-1, "sbrk (child)");
    exit();
  }
  check((wait() >= 0), "wait");

  check(a == 3, "a changed");
  check(buf[0] == 0, "buf changed");
  for (i = 0; i < npages*4096; i++) {
    check(mem[i] == 0, "mem changed");
  }
  check(sbrk(-(npages *4096)) != (char*)-1, "sbrk (parent)");

  printf(1, "ok\n");
}

// Writes by parent after fork() shouldn't be visible by child.
void
forkwrite2(void)
{
  int a = 3;
  int pid;
  int fds[2];
  char *mem;

  printf(1, "forkwrite2 test\n");

  check((mem = sbrk(4096)) != (char*)-1, "sbrk (1)");
  memset(mem, 0, 4096);

  check((pipe(fds) == 0), "pipe");
  check((pid = fork()) >= 0, "fork");

  if (pid == 0) {
    char c;
    int i;

    check(close(fds[1]) == 0, "close");

    // Wait for parent.
    check(read(fds[0], &c, 1) >= 0, "read");

    check(a == 3, "a changed");
    check(buf[0] == 0, "buf changed");
    for (i = 0; i < 4096; i++) {
      check(mem[i] == 0, "mem changed");
    }
    check(close(fds[0]) == 0, "close");
    check(sbrk(-4096) != (char*)-1, "sbrk (child)");
    exit();
  }
  check(!close(fds[0]), "close");

  // Mutate memory.
  a = 4;
  memset(buf, 1, sizeof(buf));
  memset(mem, 1, 4096);

  // Signal to child to check the values.
  check(write(fds[1], "a", 1) == 1, "write");
  check(wait() >= 0, "wait");
  check(close(fds[1]) == 0, "close");
  check(sbrk(-4096) != (char*)-1, "sbrk (2)");

  // Change buf back to inital value.
  memset(buf, 0, sizeof(buf));

  printf(1, "ok\n");
}

// Unsynchronized accesses by parent and child shouldn't
// step on eachother.
void
forkrace(void)
{
  int a;
  int pid;
  char *mem;
  int i;
  int j;
  int iters = 1;

  printf(1, "forkrace test\n");

  check((mem = sbrk(4096)) != (char*)-1, "sbrk");
  for (i = 0; i < iters; i++) {
    a = 3;
    memset(mem, 0, 4096);

    check((pid = fork()) >= 0, "fork");

    if (pid == 0) {
      check(a == 3, "a changed (child)");
      check(mem[0] == 0, "mem changed (child 1)");
      memset(mem, 1, 4096);
      for (j = 0; j < 1000; j++) {
        a = j;
        check(a == j, "a changed (child 2)");
      }
      for (j = 0; j < 64; j++) {
        int val = j % 2 ? 3 : 4;
        memset(mem, val, 4096);
        for (i = 0; i < 4096; i++) {
          check(mem[i] == val, "mem changed (child 2)");
        }
      }
      exit();
    }

    check(a == 3, "a changed (parent)");
    check(mem[0] == 0, "mem changed (parent)");
    for (j = 0; j < 500; j++) {
      a = j + 10000;
      check(a == (j + 10000), "a changed (parent 2)");
    }
    for (j = 0; j < 64; j++) {
      int val = j % 2 ? 5 : 6;
      memset(mem, val, 4096);
      check(mem[0] == val, "mem changed (parent 1)");
      check(mem[2000] == val, "mem changed (parent 2)");
      check(mem[4095] == val, "mem changed (parent 3)");
    }
    check(wait() >= 0, "wait");
  }

  check(sbrk(-4096) != (char*)-1, "sbrk (2)");

  printf(1, "ok\n");
}

// Multiple levels of forks.
void
forkdeep(void)
{
  int a = 3;
  int depth = 3;
  int breadth = 3;
  int pid;

  printf(1, "forkdeep test\n");

  check((pid = fork()) >= 0, "fork");

  if (pid == 0) {
    int i;

    while (depth--) {
      for (i = 0; i < breadth; i++) {
        check((pid = fork()) >= 0, "fork");
        if (pid == 0)
          break;
      }
      if (pid > 0)
        break;
    }
    check(buf[0] == 0, "buf changed (1)");
    check(a == 3, "a changed (1)");
    pid = getpid();
    sleep(pid / 8 + 1);
    check(buf[0] == 0, "buf changed (2)");
    check(a == 3, "a changed (2)");
    for (i = 0; i < 100; i++) {
      buf[0] = i;
      check(buf[0] == i, "buf changed (3)");
    }
    for (i = 0; i < 10000; i++) {
      a = i + depth + pid;
      check(a == i + depth + pid, "a changed(3)");
    }
    while (wait() >= 0)
      ;
    exit();
  }
  check(wait() >= 0, "wait");

  printf(1, "ok\n");
}

void
exectest(void)
{
  int pid;

  printf(1, "exectest\n");
  printf(1, "should see: 'hello world'\n");

  check((pid = fork()) >= 0, "fork");
  if (pid == 0) {
    char *argv[] = {"echo", "hello", "world", 0};
    exec("echo", argv);
    check(0, "exec failed");
  }
  check(wait() > 0, "wait");

  printf(1, "ok\n");
}

int
main(void)
{
  printf(1, "vmtest\n");

  forkread();
  forkwrite1();
  forkwrite2();
  forkrace();
  forkdeep();
  exectest();

  printf(1, "PASS\n");
  exit();
}
