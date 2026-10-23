#include <windows.h>
void mainCRTStartup(void) {
  static const char message[] = "Hello from ExeCon!\n";
  DWORD written = 0;
  HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
  if (!WriteFile(out, message, sizeof(message) - 1, &written, 0))
    ExitProcess(1);
  ExitProcess(written == sizeof(message) - 1 ? 0 : 2);
}
