#include "engine/api/execon_engine.h"
#include "engine/nt/text.h"
#include "engine/win32/modules.h"
#include "tests/unit/test.h"
using namespace execon;
TEST(unicode_roundtrip_and_rejection) {
  std::string value = "Unicode: \xce\xbb \xf0\x9f\x9a\x80";
  CHECK(nt::utf8(nt::utf16(value)) == value);
  THROWS(nt::utf16("\xc0\x80"));
  THROWS(nt::utf16("\xed\xa0\x80"));
  THROWS(nt::utf16("\xf4\x90\x80\x80"));
  THROWS(nt::utf16("\xe2"));
  THROWS(nt::utf8(std::u16string{char16_t(0xd800)}));
}
TEST(windows_command_line_quoting) {
  CHECK(nt::command_line({"app.exe", "", "a b"}) == R"(app.exe "" "a b")");
  CHECK(nt::command_line({"c\"d"}) == R"("c\"d")");
  std::string trailing = "trailing space ";
  trailing += char(92);
  std::string expected(1, '"');
  expected += trailing;
  expected += char(92);
  expected += '"';
  CHECK(nt::command_line({trailing}) == expected);
}
TEST(module_resolution_is_case_sensitive_for_symbols) {
  CHECK(win32::Modules::implemented("KERNEL32.dll", "WriteFile"));
  CHECK(win32::Modules::implemented("api-ms-win-core-file-l1-1-0.dll", "WriteFile"));
  CHECK(!win32::Modules::implemented("kernel32.dll", "writefile"));
  CHECK(!win32::Modules::implemented("unknown.dll", "WriteFile"));
  CHECK(!win32::Modules::implemented("user32.dll", "MessageBoxW"));
}
TEST(c_api_contains_errors) {
  char error[64]{};
  uint32_t code = 0;
  CHECK(execon_run(nullptr, 0, nullptr, 1, &code, error, sizeof(error)) == -1);
  CHECK(std::string(error) == "invalid C API arguments");
}
