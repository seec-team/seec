#include <stdio.h>

int main(int argc, char *argv[])
{
  int index = atoi(argv[1]);
  char **ptr = argv + index;
  char *str = *ptr;
  return 0;
}

