#include <stdio.h>

char *get_data()
{
  return "012345678";
}

int main(int argc, char *argv[])
{
  int index = atoi(argv[1]);
  char *ptr = get_data() + index;
  char c    = *ptr;
  return 0;
}
