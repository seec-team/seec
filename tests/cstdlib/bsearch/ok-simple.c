#include <stdlib.h>

struct foo {
  int a;
  int b;
};

int foo_less_a(void const *left, void const *right) {
  return ((struct foo const *)left)->a -
         ((struct foo const *)right)->a;
}

int main(int argc, char *argv[]) {
  struct foo array[] = {
    { .a =  2, .b = 1 },
    { .a =  3, .b = 2 },
    { .a =  4, .b = 3 },
    { .a =  7, .b = 3 },
    { .a = 10, .b = 3 }
  };

  int searchFor = atoi(argv[1]);

  struct foo *elem = bsearch(&(struct foo){ .a = searchFor, .b = 0 },
                             array,
                             sizeof(array)/sizeof(array[0]),
                             sizeof(array[0]),
                             foo_less_a);

  exit(elem == &array[2] ? EXIT_SUCCESS : EXIT_FAILURE);
}
