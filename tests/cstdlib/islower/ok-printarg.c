#include <ctype.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
  for (int i = 0; argv[0][i]; ++i)
    if (islower(argv[0][i]))
      printf("%c\n", argv[0][i]);

  return 0;
}

