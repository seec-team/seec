#include <stdio.h>
#include <stdlib.h>

static int const *iptr;

void foo()
{
  int a = 0;
  iptr = &a;
  printf("   &a = %p\n",    &a);
  printf("    a = %d\n",     a);
  printf(" iptr = %p\n",  iptr);
  printf("*iptr = %d\n", *iptr);
}

void bar()
{
  int a = 0;
  printf("   &a = %p\n",    &a);
  printf("    a = %d\n",     a);
  printf(" iptr = %p\n",  iptr);
  printf("*iptr = %d\n", *iptr);
}

int main(int argc, char *argv[])
{
  foo();
  bar();
  exit(EXIT_SUCCESS);
}
