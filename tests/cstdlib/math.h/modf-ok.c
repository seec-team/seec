#include <math.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
  float       intpartf;
  double      intpart;
  long double intpartl;

  float       fracf = modff(100.0f, &intpartf);
  double      frac  = modf( 100.0 , &intpart );
  long double fracl = modfl(100.0l, &intpartl);

  printf("integer parts: %f, %f, %Lf.\n", intpartf, intpart, intpartl);

  return 0;
}

