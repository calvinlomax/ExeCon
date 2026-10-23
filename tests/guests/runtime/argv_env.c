#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
  const char *value = getenv("EXECON_TEST");
  if (argc != 2 || !value)
    return 1;
  printf("argc=%d\narg=%s\nenv=%s\n", argc, argv[1], value);
  return 0;
}
