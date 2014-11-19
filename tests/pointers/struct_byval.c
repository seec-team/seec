#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Foo {
  int number;
  char *string;
};

void lookFoo(struct Foo TheFoo) {
  printf("Foo.string: %s\n", TheFoo.string);
}

int main(int argc, char *argv[], char *envp[])
{
  if (argc != 2)
    exit(EXIT_FAILURE);

  if (strcmp("valid", argv[1]) == 0) {
    lookFoo((struct Foo){ .number = 0, .string = argv[1] });
  }
  else {
    fprintf(stderr, "unknown action: %s\n", argv[1]);
    exit(EXIT_FAILURE);
  }

  exit(EXIT_SUCCESS);
}
