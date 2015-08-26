#include <stdio.h>

struct Foo {
  int a;
  double b;
  char c;
};

void lookFoo(struct Foo TheFoo) {
  printf("Foo.a: %i\n", TheFoo.a);
}

int main(int argc, char *argv[]) {
  lookFoo((struct Foo){ .a = 5, .b = 10.0, .c = 'c' });
  return 0;
}

