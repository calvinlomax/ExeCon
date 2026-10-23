#include <stdio.h>
#include <windows.h>
int main(void) {
  WIN32_FIND_DATAW data;
  HANDLE h = FindFirstFileW(L"T:\\*.txt", &data);
  if (h == INVALID_HANDLE_VALUE)
    return 1;
  unsigned count = 0;
  do {
    if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
      ++count;
  } while (FindNextFileW(h, &data));
  if (GetLastError() != ERROR_NO_MORE_FILES || !FindClose(h))
    return 2;
  printf("files=%u\n", count);
  return count == 1 ? 0 : 3;
}
