#include <errno.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
  errno = 0;
  printf("errno: %d\n", errno);
  return 0;
}

