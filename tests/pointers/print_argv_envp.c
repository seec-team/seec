#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[], char *envp[])
{
  for (char **p = argv; *p != NULL; ++p)
    printf("arg: %s\n", *p);

  for (char **p = envp; *p != NULL; ++p)
    printf("env: %s\n", *p);

  exit(EXIT_SUCCESS);
}
