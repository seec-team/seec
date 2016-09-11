#include <string.h>

void bar(int a, char buffer[a])
{
  for (int i = 1; i < a; ++i)
  {
    char copy[i];
    memcpy(copy, buffer, i);
  }
}

void foo(int b)
{
  for (int i = 1; i < b; ++i)
  {
    char buffer[i];
    for (int j = 0; j < i; ++j)
    {
      buffer[j] = j;
    }
    bar(i, buffer);
  }
}

int main(int argc, char *argv[])
{
  foo(3);
  return 0;
}
