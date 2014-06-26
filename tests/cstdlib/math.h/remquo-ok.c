#include <math.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
  int quof, quo, quol;

  float       rf = remquof(100.0f, 30.0f, &quof);
  double      r  = remquo( 100.0 , 30.0 , &quo );
  long double rl = remquol(100.0l, 30.0l, &quol);

  printf("quo: %d, %d, %d.\n", quof, quo, quol);

  return 0;
}

