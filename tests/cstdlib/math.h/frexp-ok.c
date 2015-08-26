#include <math.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
  int ef, e, el;

  float       sigf = frexpf(100.0f, &ef);
  double      sig  = frexp( 100.0 , &e );
  long double sigl = frexpl(100.0l, &el);

  printf("exponents: %d, %d, %d.\n", ef, e, el);

  return 0;
}

