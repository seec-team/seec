long double foo(long double value)
{
  long double const copy = value;
  return copy;
}

int main(int argc, char *argv[]) {
  long double const zero = 0.0L;
  long double const zero_copy = foo(zero);

  long double const positive = 3.14159265358979323846264338328L;
  long double const positive_copy = foo(positive);

  long double const negative = -3.14159265358979323846264338328L;
  long double const negative_copy = foo(negative);

  return 0;
}
