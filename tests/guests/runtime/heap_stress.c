#include <stdio.h>
#include <windows.h>
int main(void) {
  HANDLE heap = GetProcessHeap();
  void *blocks[128];
  for (unsigned round = 0; round < 8; ++round) {
    for (unsigned i = 0; i < 128; ++i) {
      unsigned char *p = HeapAlloc(heap, HEAP_ZERO_MEMORY, 17 + i);
      if (!p || p[i])
        return 1;
      p[i] = (unsigned char)i;
      blocks[i] = p;
    }
    for (unsigned i = 0; i < 128; ++i) {
      unsigned char *p = HeapReAlloc(heap, HEAP_ZERO_MEMORY, blocks[i], 512);
      if (!p || p[i] != (unsigned char)i || p[511])
        return 2;
      if (!HeapFree(heap, 0, p))
        return 3;
    }
  }
  puts("heap OK");
  return 0;
}
