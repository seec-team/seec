#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Foo {
  int length;
  char buffer[10];
};

struct Foo fooarray[10];

int main(int argc, char *argv[])
{
  strcpy(fooarray[0].buffer, "hello");
  strcpy(fooarray[1].buffer, "world");
  strcpy(&(fooarray[2].buffer[1]), "test");

  return 0;
}
