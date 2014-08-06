#include <stdlib.h>
#include <string.h>

char *test_size(char *p, size_t size)
{
  p = realloc(p, size);

  if (p != NULL && size != 0)
    memset(p, 0, size);

  return p;
}

int main(int argc, char *argv[])
{
  char *p = NULL;

  p = test_size(p, 0);
  p = test_size(p, 1);
  p = test_size(p, 8);
  p = test_size(p, 128);
  p = test_size(p, 1024);
  p = test_size(p, 16384);
  p = test_size(p, 8);

  exit(EXIT_SUCCESS);
}

