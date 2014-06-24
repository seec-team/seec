#include <stdlib.h>

struct foo {
  int a;
  int b;
};

int foo_less_a(void const *left, void const *right) {
  return ((struct foo const *)left)->a -
         ((struct foo const *)right)->b;
}

int main(int argc, char *argv[]) {
  struct foo array[] = {
    { .a = 5, .b = 1 },
    { .a = 3, .b = 2 },
    { .a = 6, .b = 3 }
  };

  qsort(array,
        sizeof(array)/sizeof(array[0]),
        sizeof(array[0]),
        foo_less_a);

  return 0;
}

