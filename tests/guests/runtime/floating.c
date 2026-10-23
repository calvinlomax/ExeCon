#include <stdio.h>
int main(void) {
  volatile double a = 1.5, b = 2.5;
  double c = a * b;
  printf("%.3f\n", c);
  return c == 3.75 ? 0 : 1;
}
