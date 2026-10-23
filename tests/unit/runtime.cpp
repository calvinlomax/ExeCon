#include "engine/api/execon_engine.h"
#include "engine/nt/text.h"
#include "engine/win32/modules.h"
#include "tests/unit/test.h"
#include <sstream>
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
TEST(paths_windows_normalization_and_devices) {
  nt::Paths paths("/tmp/execon-prefix", "C:\\Work", {{'T', "/tmp"}});
  CHECK(paths.absolute("..\\File.txt") == "C:\\File.txt");
  CHECK(paths.resolve("C:\\..\\..\\File.txt") == "/tmp/execon-prefix/drive_c/File.txt");
  CHECK(paths.resolve("NUL") == "/dev/null");
  CHECK(paths.resolve("\\\\?\\T:\\hello") == "/tmp/hello");
  CHECK(paths.resolve("Z:\\tmp") == "/tmp");
  THROWS(paths.resolve("\\\\server\\share"));
  THROWS(paths.resolve("C:relative"));
  THROWS(paths.resolve("C:\\NUL\\child"));
  THROWS(paths.resolve("X:\\unknown"));
  THROWS(paths.resolve("CON.txt:stream"));
}
TEST(heap_realloc_and_invalid_free) {
  mem::AddressSpace memory;
  nt::Heap heap(memory);
  auto p = heap.allocate(3);
  memory.store(p, 0xabcdef, 2);
  auto q = heap.reallocate(p, 8192);
  CHECK(memory.load(q, 2) == 0xcdef);
  CHECK(memory.load(q + 4096, 1) == 0);
  THROWS(memory.load(p, 1));
  THROWS(heap.free(p));
  CHECK(heap.size(q) == 8192);
  heap.free(q);
  THROWS(memory.load(q, 1));
}
TEST(file_handles_sharing_and_case_lookup) {
  auto root = std::filesystem::temp_directory_path() /
              ("execon-unit-" + std::to_string(nt::Services().process_id()));
  struct Cleanup {
    std::filesystem::path path;
    ~Cleanup() {
      std::error_code ec;
      std::filesystem::remove_all(path, ec);
    }
  } cleanup{root};
  std::filesystem::create_directories(root / "drive_c");
  nt::Services files{nt::Paths(root)};
  auto h = files.open("C:\\MixedCase.dat", 0xc0000000, 0, 1);
  std::array<uint8_t, 3> data = {0, 0xff, 42};
  CHECK(files.write(h, data) == 3);
  THROWS(files.open("c:\\mixedcase.DAT", 0x80000000, 3, 3));
  CHECK(files.file_size(h) == 3);
  CHECK(files.seek(h, 0, 0) == 0);
  CHECK(files.read(h, 3) == std::vector<uint8_t>(data.begin(), data.end()));
  files.close(h);
  THROWS(files.close(h));
  auto read = files.open("c:\\mixedcase.DAT", 0x80000000, 1, 3);
  CHECK(files.read(read, 3).size() == 3);
  files.close(read);
}

TEST(import_thunks_are_not_fake_data_objects) {
  mem::AddressSpace memory;
  memory.map(0x1000, 4096, mem::Read | mem::Write);
  nt::Heap heap(memory);
  nt::Services services;
  win32::ProcessState state;
  std::ostringstream log;
  win32::Modules modules(services, heap, state, log, win32::MissingPolicy::Abort);
  pe::Image image;
  image.is64 = true;
  image.imports.push_back({"kernel32.dll", "ExitProcess", 0, 0x100, false});
  modules.bind(image, 0x1000, memory);
  auto token = memory.load(0x1100, 8);
  CHECK(modules.contains(token));
  THROWS(memory.load(token, 1));
  CHECK(memory.load(token, 2, mem::Execute) == 0x0b0f);
}
