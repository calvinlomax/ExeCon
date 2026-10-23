#include <windows.h>
void mainCRTStartup(void) {
  HANDLE heap = GetProcessHeap();
  char *p = HeapAlloc(heap, HEAP_ZERO_MEMORY, 32);
  if (!p || p[0])
    ExitProcess(1);
  p[0] = 'E';
  p[1] = 'x';
  p[2] = 'e';
  p[3] = 'C';
  p[4] = 'o';
  p[5] = 'n';
  p[6] = '\n';
  p = HeapReAlloc(heap, HEAP_ZERO_MEMORY, p, 64);
  if (!p || p[0] != 'E' || p[40])
    ExitProcess(2);
  HANDLE f = CreateFileW(L"T:\\RoundTrip.bin", GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, 0);
  if (f == INVALID_HANDLE_VALUE)
    ExitProcess(3);
  DWORD count = 0;
  if (!WriteFile(f, p, 7, &count, 0) || count != 7)
    ExitProcess(4);
  LARGE_INTEGER zero;
  zero.QuadPart = 0;
  if (!SetFilePointerEx(f, zero, 0, FILE_BEGIN))
    ExitProcess(5);
  char data[8];
  if (!ReadFile(f, data, 7, &count, 0) || count != 7 || data[0] != 'E')
    ExitProcess(6);
  if (!CloseHandle(f) || !HeapFree(heap, 0, p))
    ExitProcess(7);
  if (!WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), data, count, &count, 0))
    ExitProcess(8);
  ExitProcess(0);
}
