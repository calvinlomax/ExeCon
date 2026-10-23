#include <locale.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  char buffer[16];
  memcpy(buffer, "strings", 8);
  if (strcmp(buffer, "strings") || !setlocale(LC_ALL, "C"))
    return 1;
  puts("C strings OK");
  return 0;
}
