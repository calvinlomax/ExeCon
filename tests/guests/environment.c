#include <windows.h>
void mainCRTStartup(void) {
  char value[64];DWORD n=GetEnvironmentVariableA("EXECON_TEST",value,sizeof(value)),written=0;
  if(n==0||n>=sizeof(value))ExitProcess(1);
  if(!WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),value,n,&written,0))ExitProcess(2);
  LARGE_INTEGER counter,frequency;
  if(!QueryPerformanceCounter(&counter)||!QueryPerformanceFrequency(&frequency)||frequency.QuadPart!=1000000000LL)ExitProcess(3);
  ExitProcess(0);
}
