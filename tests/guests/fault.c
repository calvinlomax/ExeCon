#include <windows.h>
void mainCRTStartup(void) { ExitProcess(*(volatile DWORD *)0x12345000); }
