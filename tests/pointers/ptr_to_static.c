#include <stdio.h>

char *get_data()
{
  static char data[10];
  return data;
}

int main(int argc, char *argv[])
{
  int index = atoi(argv[1]);
  char *ptr = get_data() + index;
  char c    = *ptr;
  return 0;
}

