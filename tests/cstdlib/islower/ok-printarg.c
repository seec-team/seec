#include <ctype.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
  for (int i = 0; argv[1][i]; ++i)
    if (islower(argv[1][i]))
      printf("%c\n", argv[1][i]);

  return 0;
}

