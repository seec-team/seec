#include <stdlib.h>
#include <string.h>

void test_size(size_t size)
{
  void *p = malloc(size);

  if (p) {
    if (size) {
      memset(p, 0, size);
    }
    free(p);
  }
}

int main(int argc, char *argv[])
{
  test_size(0);
  test_size(1);
  test_size(8);
  test_size(128);
  test_size(1024);
  test_size(16384);
  test_size(17);

  exit(EXIT_SUCCESS);
}

