
#include "types.h"
#include "user.h"
#include "fcntl.h"

char buf[8192] = {1};
char buf2[8192] = {2};

int
main(void)
{
  printf(1, "myprog\n");

  int fd = open("hello.txt", O_CREATE|O_RDWR);
  if (fd < 0) {
    printf(2, "error: open\n");
    exit();
  }

  int i;
  for (i = 0; i < sizeof(buf); i++) {
    buf[i] = 'a' + (i % 64);
  }

  for (i = 0; i < sizeof(buf2); i++) {
    buf2[i] = 'z';
  }

  if (write(fd, buf, sizeof(buf)) != sizeof(buf)) {
    printf(2, "error: short write\n");
    exit();
  }

  close(fd);

  fd = open("hello.txt", O_RDONLY);
  if (fd < 0) {
    printf(2, "error: open 2\n");
    exit();
  }

  int n = 0;
  int nr;
  while (1) {
    nr = read(fd, buf2 + n, sizeof(buf2) - n);
    if (nr <= 0) {
        break;
    }
    n += nr;
  }
  if (nr < 0) {
    printf(2, "error read\n");
    exit();
  }
  printf(1, "buf2 is at %p\n", buf2);
  printf(1, "read n=%d\n", n);
  printf(1, "buf[0]=%c, buf2[0]=%c\n", 
      buf[0], buf2[0]);
  buf[sizeof(buf)-1] = 0;
  buf2[sizeof(buf2)-1] = 0;
  if (strcmp(buf, buf2) != 0) {
    printf(2, "read bad char\n");
    for (i = 0; i < sizeof(buf); i++) {
      if (buf[i] != buf2[i]) {
        printf(1, "bad char at pos %d. b=%c. b2=%c\n", i, buf[i], buf2[i]);
      }
    }
    exit();
  }

  printf(1, "success\n");
  close(fd);
  exit();
}

