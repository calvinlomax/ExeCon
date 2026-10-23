#include <stdio.h>
#include <windows.h>
static volatile LONG counter;
static DWORD WINAPI worker(void *unused) {
  (void)unused;
  for (unsigned i = 0; i < 1000; ++i)
    InterlockedIncrement(&counter);
  return 0;
}
int main(void) {
  HANDLE a = CreateThread(0, 0, worker, 0, 0, 0), b = CreateThread(0, 0, worker, 0, 0, 0);
  if (!a || !b)
    return 1;
  if (WaitForSingleObject(a, 5000) != WAIT_OBJECT_0 ||
      WaitForSingleObject(b, 5000) != WAIT_OBJECT_0)
    return 2;
  CloseHandle(a);
  CloseHandle(b);
  printf("counter=%ld\n", counter);
  return counter == 2000 ? 0 : 3;
}
