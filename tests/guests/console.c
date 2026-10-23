#include <windows.h>
void mainCRTStartup(void) {
  static const WCHAR text[] = L"Unicode: \x03bb \xd83d\xde80\n";
  DWORD written = 0;
  SetLastError(1234);
  if (GetLastError() != 1234)
    ExitProcess(1);
  if (!WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), text, sizeof(text) / 2 - 1, &written, 0))
    ExitProcess(2);
  ExitProcess(0);
}
