#include <stddef.h>
__declspec(dllimport) void __stdcall ExitProcess(unsigned);
__declspec(dllimport) int __cdecl puts(const char *);
__declspec(dllimport) void *__cdecl malloc(size_t);
__declspec(dllimport) void __cdecl free(void *);
void mainCRTStartup(void) {
  char *p=malloc(16);if(!p)ExitProcess(1);p[0]='C';p[1]='R';p[2]='T';p[3]=0;
  puts(p);free(p);ExitProcess(0);
}
