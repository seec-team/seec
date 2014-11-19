#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Foo {
  int number;
  char *string;
};

struct Foo makeFoo(char *string) {
  return (struct Foo){ .number = 0, .string = string };
}

int main(int argc, char *argv[], char *envp[])
{
  if (argc != 2)
    exit(EXIT_FAILURE);

  if (strcmp("valid", argv[1]) == 0) {
    struct Foo TheFoo = makeFoo(argv[1]);
    printf("TheFoo.string = %s\n", TheFoo.string);
  }
  else {
    fprintf(stderr, "unknown action: %s\n", argv[1]);
    exit(EXIT_FAILURE);
  }

  exit(EXIT_SUCCESS);
}
