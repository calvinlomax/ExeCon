#include <stdio.h>
#include <windows.h>
int main(void) {
  HKEY key;
  DWORD data = 42, out = 0, size = sizeof(out), type = 0;
  if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\ExeConTest", 0, 0, 0, KEY_READ | KEY_WRITE, 0,
                      &key, 0))
    return 1;
  if (RegSetValueExW(key, L"Value", 0, REG_DWORD, (const BYTE *)&data, sizeof(data)))
    return 2;
  if (RegQueryValueExW(key, L"Value", 0, &type, (BYTE *)&out, &size) || type != REG_DWORD ||
      out != 42)
    return 3;
  RegCloseKey(key);
  puts("registry OK");
  return 0;
}
