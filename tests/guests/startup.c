#include <intrin.h>
#include <windows.h>
void mainCRTStartup(void) {
#ifdef _WIN64
  ULONG_PTR teb = __readgsqword(0x30), peb = __readgsqword(0x60);
  if (*(ULONG_PTR *)(teb + 0x30) != teb ||
      *(ULONG_PTR *)(peb + 0x10) != (ULONG_PTR)GetModuleHandleW(0))
    ExitProcess(1);
#else
  ULONG_PTR teb = __readfsdword(0x18), peb = __readfsdword(0x30);
  if (*(ULONG_PTR *)teb != 0xffffffff || *(ULONG_PTR *)(teb + 0x18) != teb ||
      *(ULONG_PTR *)(peb + 8) != (ULONG_PTR)GetModuleHandleW(0))
    ExitProcess(1);
#endif
  static const char text[] = "TEB/PEB OK\n";
  DWORD written;
  WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text, sizeof(text) - 1, &written, 0);
  ExitProcess(0);
}
