#include <windows.h>
void mainCRTStartup(void) {
  __asm__ volatile("ud2");
  ExitProcess(0);
}
