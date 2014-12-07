#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
  int n = atoi(argv[1]);
  char buffer[n+1];

  strncpy(buffer, argv[2], n);
  buffer[n] = '\0';

  fputs(buffer, stdout);

  return 0;
}
