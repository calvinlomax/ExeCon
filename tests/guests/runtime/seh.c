#include <stdio.h>
#include <windows.h>
int main(void) {
  int caught = 0;
  __try {
    RaiseException(0xe0420001, 0, 0, 0);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    caught = 1;
  }
  if (!caught)
    return 1;
  puts("SEH caught");
  return 0;
}
