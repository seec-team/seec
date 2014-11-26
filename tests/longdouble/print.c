#include <stdio.h>

int main(int argc, char *argv[]) {
  long double const zero = 0.0L;
  printf("%Lf\n", zero);

  long double const positive = 3.14159265358979323846264338328L;
  printf("%Lf\n", positive);

  long double const negative = -3.14159265358979323846264338328L;
  printf("%Lf\n", negative);

  return 0;
}
