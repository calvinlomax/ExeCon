#include <windows.h>
void mainCRTStartup(void) { MessageBoxW(0,L"Deferred GUI",L"ExeCon",0); ExitProcess(0); }
