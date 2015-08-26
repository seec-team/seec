#include <math.h>
#include <stdlib.h>

void do_nan_string(char const * const arg)
{
  float       nf = nanf(arg);
  double      n  = nan( arg);
  long double nl = nanl(arg);
}

int main(int argc, char *argv[]) {
  do_nan_string("");
  do_nan_string("string");
  return 0;
}

