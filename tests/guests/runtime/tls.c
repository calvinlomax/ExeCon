#include <stdio.h>
static _Thread_local int value = 41;
int main(void) {
  if (value != 41)
    return 1;
  ++value;
  printf("tls=%d\n", value);
  return 0;
}
