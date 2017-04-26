#include <string.h>

char *foo() {
  char buffer[10];
  char *str = strcpy(buffer, "Hello");
  return str;
}

int main(int argc, char *argv[])
{
  char *str = foo();
  strcat(str, "!");
  return 0;
}

