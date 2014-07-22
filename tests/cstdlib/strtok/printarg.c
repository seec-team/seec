#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
  char const * const delim = ":";
  char *s = strtok(argv[1], delim);

  while (s) {
    printf("%s\n", s);
    s = strtok(NULL, delim);
  }

  return 0;
}

