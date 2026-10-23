#include <windows.h>
void mainCRTStartup(void) {
  char buffer[64];
  DWORD count = 0, written = 0;
  if (!ReadFile(GetStdHandle(STD_INPUT_HANDLE), buffer, sizeof(buffer), &count, 0))
    ExitProcess(1);
  if (!WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buffer, count, &written, 0))
    ExitProcess(2);
  ExitProcess(written == count ? 0 : 3);
}
