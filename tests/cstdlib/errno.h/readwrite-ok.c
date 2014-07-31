#include <errno.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
  printf("errno: %d\n", errno);
  errno = 0;
  printf("errno: %d\n", errno);
  return 0;
}

