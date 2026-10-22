#include "engine/win32/modules.h"
#include "engine/nt/text.h"
#include <algorithm>
#include <bit>
#include <cctype>
#include <chrono>
#include <set>
namespace execon::win32 {
namespace {
std::string canonical(std::string dll) {
  for (auto &c : dll)
    c = char(std::tolower(static_cast<unsigned char>(c)));
  if (dll.starts_with("api-ms-win-core-") || dll == "kernelbase.dll")
    return "kernel32.dll";
  if (dll.starts_with("api-ms-win-crt-"))
    return "ucrtbase.dll";
  return dll;
}
bool known_error(const std::string &n) {
  return n == "MessageBoxW" || n == "MessageBoxA" || n == "CreateWindowExW" ||
         n == "CreateWindowExA" || n == "GetMessageW" || n == "GetMessageA";
}
uint64_t argument(cpu::Context &c, mem::AddressSpace &m, unsigned index) {
  if (!c.mode64)
    return m.load(c.gpr[cpu::RSP] + 4 + index * 4, 4);
  constexpr unsigned regs[] = {cpu::RCX, cpu::RDX, cpu::R8, cpu::R9};
  return index < 4 ? c.gpr[regs[index]] : m.load(c.gpr[cpu::RSP] + 40 + (index - 4) * 8, 8);
}
} // namespace
std::string read_string(mem::AddressSpace &m, uint64_t a, bool wide) {
  std::string bytes;
  std::u16string chars;
  for (size_t i = 0; i < 32768; ++i) {
    auto v = m.load(a + i * (wide ? 2 : 1), wide ? 2 : 1);
    if (!v)
      return wide ? nt::utf8(chars) : bytes;
    if (wide)
      chars += char16_t(v);
    else
      bytes += char(v);
  }
  throw nt::Error(87, "unterminated guest string");
}
bool Modules::implemented(const std::string &dll, const std::string &name) {
  auto module = canonical(dll);
  static const std::set<std::string> kernel = {"ExitProcess",
                                               "GetStdHandle",
                                               "WriteFile",
                                               "ReadFile",
                                               "WriteConsoleA",
                                               "WriteConsoleW",
                                               "GetLastError",
                                               "SetLastError",
                                               "GetCommandLineA",
                                               "GetCommandLineW",
                                               "GetCurrentProcessId",
                                               "GetCurrentThreadId",
                                               "GetModuleHandleA",
                                               "GetModuleHandleW",
                                               "CloseHandle",
                                               "CreateFileA",
                                               "CreateFileW",
                                               "SetFilePointerEx",
                                               "GetFileSizeEx",
                                               "GetProcessHeap",
                                               "HeapAlloc",
                                               "HeapFree",
                                               "HeapReAlloc",
                                               "HeapSize",
                                               "GetEnvironmentVariableA",
                                               "GetEnvironmentVariableW",
                                               "GetCurrentDirectoryA",
                                               "GetCurrentDirectoryW",
                                               "QueryPerformanceCounter",
                                               "QueryPerformanceFrequency",
                                               "GetACP",
                                               "GetConsoleOutputCP"};
  static const std::set<std::string> crt = {"puts",   "strlen", "memcpy", "memmove",
                                            "memset", "exit",   "_exit",  "malloc",
                                            "calloc", "free",   "realloc"};
  return (module == "kernel32.dll" && kernel.contains(name)) ||
         ((module == "ucrtbase.dll" || module == "msvcrt.dll") && crt.contains(name)) ||
         (module == "ntdll.dll" && name == "RtlExitUserProcess");
}
void Modules::report(const pe::Image &p, std::ostream &out) {
  for (auto &i : p.imports)
    out << (implemented(i.dll, i.name) ? "implemented " : "MISSING ") << i.dll << '!' << i.name
        << (i.delay ? " (delay)" : "") << '\n';
}
void Modules::bind(const pe::Image &p, uint64_t base, mem::AddressSpace &m) {
  constexpr uint64_t stub_base = 0x70000000;
  uint64_t size = ((p.imports.size() * 16 + 4095) / 4096) * 4096;
  if (!size)
    return;
  m.map(stub_base, size, mem::Read | mem::Write);
  uint64_t next = stub_base;
  for (auto &i : p.imports) {
    stubs_.emplace(next, i);
    m.store(next, 0x0b0f, 2);
    auto a = base + i.iat, page = a / 4096 * 4096;
    auto flags = m.permissions(a);
    m.protect(page, 4096, flags | mem::Write);
    m.store(a, next, p.is64 ? 8 : 4);
    m.protect(page, 4096, flags);
    next += 16;
  }
  m.protect(stub_base, size, mem::Read | mem::Execute);
}
uint64_t Modules::invoke(const pe::Import &api, cpu::Context &c, mem::AddressSpace &m) {
  auto arg = [&](unsigned n) { return argument(c, m, n); };
  auto &name = api.name;
  auto error = [&](uint32_t code) { m.store(state_.teb + (c.mode64 ? 0x68 : 0x34), code, 4); };
  if (!implemented(api.dll, name)) {
    log_ << "missing API " << api.dll << '!' << name << " return-address=0x" << std::hex
         << m.load(c.gpr[cpu::RSP], c.mode64 ? 8 : 4) << std::dec << '\n';
    if (policy_ == MissingPolicy::ReturnError && known_error(name)) {
      error(120);
      if (name == "GetMessageW" || name == "GetMessageA")
        return UINT64_MAX;
      return 0;
    }
    if (policy_ == MissingPolicy::IgnoreVoid && canonical(api.dll) == "kernel32.dll" &&
        name == "Sleep") {
      error(120);
      return 0;
    }
    throw std::runtime_error("unimplemented import " + api.dll + "!" + name +
                             " (no safe return contract for selected policy)");
  }
  if (name == "ExitProcess" || name == "RtlExitUserProcess" || name == "exit" || name == "_exit") {
    state_.exited = true;
    state_.exit_code = uint32_t(arg(0));
    return 0;
  }
  if (name == "GetStdHandle")
    return services_.standard_handle(int32_t(uint32_t(arg(0))));
  if (name == "GetLastError")
    return m.load(state_.teb + (c.mode64 ? 0x68 : 0x34), 4);
  if (name == "SetLastError") {
    error(uint32_t(arg(0)));
    return 0;
  }
  if (name == "GetCommandLineA")
    return state_.command_a;
  if (name == "GetCommandLineW")
    return state_.command_w;
  if (name == "GetCurrentProcessId")
    return services_.process_id();
  if (name == "GetCurrentThreadId")
    return 1;
  if (name == "GetModuleHandleA" || name == "GetModuleHandleW") {
    if (arg(0))
      throw nt::Error(126, "named module lookup not implemented");
    return state_.image_base;
  }
  if (name == "CloseHandle") {
    services_.close(arg(0));
    return 1;
  }
  if (name == "WriteFile" || name == "WriteConsoleA" || name == "WriteConsoleW" ||
      name == "ReadFile") {
    uint64_t h = arg(0), buffer = arg(1), count = uint32_t(arg(2)), written = arg(3),
             overlap = arg(4);
    if (overlap)
      throw nt::Error(120, "overlapped/reserved console I/O unsupported");
    if (count > 16 * 1024 * 1024)
      throw nt::Error(8, "single I/O exceeds 16 MiB limit");
    if (written) {
      m.check(written, 4, mem::Write);
      m.store(written, 0, 4);
    } else if (name == "WriteFile" || name == "ReadFile")
      throw nt::Error(87, "synchronous file I/O needs count pointer");
    size_t done = 0;
    if (name == "ReadFile") {
      m.check(buffer, count, mem::Write);
      auto bytes = services_.read(h, count);
      m.write(buffer, bytes);
      done = bytes.size();
    } else if (name == "WriteConsoleW") {
      m.check(buffer, count * 2, mem::Read);
      std::u16string chars;
      for (uint64_t i = 0; i < count; ++i)
        chars += char16_t(m.load(buffer + i * 2, 2));
      auto text = nt::utf8(chars);
      services_.write(h, std::span(reinterpret_cast<const uint8_t *>(text.data()), text.size()));
      done = count;
    } else
      done = services_.write(h, m.read(buffer, count));
    if (written)
      m.store(written, done, 4);
    return 1;
  }

  if (name == "CreateFileA" || name == "CreateFileW") {
    auto flags = uint32_t(arg(5));
    if (arg(3) || arg(6) || (flags & ~0x80U))
      throw nt::Error(120, "security/template/extended file flags not implemented");
    return services_.open(read_string(m, arg(0), name == "CreateFileW"), uint32_t(arg(1)),
                          uint32_t(arg(2)), uint32_t(arg(4)));
  }
  if (name == "SetFilePointerEx") {
    auto pointer = arg(c.mode64 ? 2 : 3), origin = arg(c.mode64 ? 3 : 4);
    uint64_t offset = arg(1);
    if (!c.mode64)
      offset |= arg(2) << 32;
    if (pointer)
      m.check(pointer, 8, mem::Write);
    auto value = services_.seek(arg(0), std::bit_cast<int64_t>(offset), uint32_t(origin));
    if (pointer)
      m.store(pointer, value, 8);
    return 1;
  }
  if (name == "GetFileSizeEx") {
    m.check(arg(1), 8, mem::Write);
    m.store(arg(1), services_.file_size(arg(0)), 8);
    return 1;
  }
  if (name == "GetProcessHeap")
    return 0x400;
  if (name == "HeapAlloc" || name == "HeapFree" || name == "HeapReAlloc" || name == "HeapSize") {
    if (arg(0) != 0x400)
      throw nt::Error(6, "only the process heap is implemented");
    if (arg(1) & ~8ULL)
      throw nt::Error(120, "heap flags are not implemented");
    if (name == "HeapAlloc")
      return heap_.allocate(arg(2));
    if (name == "HeapFree") {
      heap_.free(arg(2));
      return 1;
    }
    if (name == "HeapSize")
      return heap_.size(arg(2));
    return heap_.reallocate(arg(2), arg(3));
  }
  if (name == "malloc")
    return heap_.allocate(arg(0));
  if (name == "calloc") {
    if (arg(0) && arg(1) > UINT64_MAX / arg(0))
      throw nt::Error(8, "calloc size overflow");
    return heap_.allocate(arg(0) * arg(1));
  }
  if (name == "free") {
    heap_.free(arg(0));
    return 0;
  }
  if (name == "realloc") {
    if (!arg(0))
      return heap_.allocate(arg(1));
    if (!arg(1)) {
      heap_.free(arg(0));
      return 0;
    }
    return heap_.reallocate(arg(0), arg(1));
  }
  if (name == "GetACP" || name == "GetConsoleOutputCP")
    return 65001;
  if (name == "QueryPerformanceCounter" || name == "QueryPerformanceFrequency") {
    uint64_t value = name == "QueryPerformanceFrequency"
                         ? 1000000000ULL
                         : uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                        std::chrono::steady_clock::now().time_since_epoch())
                                        .count());
    m.store(arg(0), value, 8);
    return 1;
  }
  if (name == "GetEnvironmentVariableA" || name == "GetEnvironmentVariableW" ||
      name == "GetCurrentDirectoryA" || name == "GetCurrentDirectoryW") {
    bool current = name.starts_with("GetCurrentDirectory"), wide = name.back() == 'W';
    std::string value;
    if (current)
      value = services_.paths().cwd();
    else {
      auto key = read_string(m, arg(0), wide);
      auto upper = [](std::string s) {
        for (auto &c : s)
          c = char(std::toupper(static_cast<unsigned char>(c)));
        return s;
      };
      bool found = false;
      for (auto &[k, v] : state_.environment)
        if (upper(k) == upper(key)) {
          value = v;
          found = true;
          break;
        }
      if (!found)
        throw nt::Error(203, "environment variable not found");
    }
    auto buffer = arg(1);
    auto capacity = uint32_t(arg(current ? 0 : 2));
    auto chars = nt::utf16(value);
    auto count = wide ? chars.size() : value.size();
    if (capacity <= count)
      return count + 1;
    m.check(buffer, (count + 1) * (wide ? 2 : 1), mem::Write);
    if (wide) {
      for (size_t n = 0; n < chars.size(); ++n)
        m.store(buffer + n * 2, chars[n], 2);
      m.store(buffer + chars.size() * 2, 0, 2);
    } else {
      m.write(buffer, std::span(reinterpret_cast<const uint8_t *>(value.data()), value.size()));
      m.store(buffer + value.size(), 0, 1);
    }
    return count;
  }
  if (name == "strlen")
    return read_string(m, arg(0)).size();
  if (name == "puts") {
    auto text = read_string(m, arg(0)) + "\n";
    services_.write(services_.standard_handle(-11),
                    std::span(reinterpret_cast<const uint8_t *>(text.data()), text.size()));
    return 0;
  }
  if (name == "memcpy" || name == "memmove" || name == "memset") {
    auto dst = arg(0), src = arg(1), n = arg(2);
    if (n > 16 * 1024 * 1024)
      throw nt::Error(8, "CRT memory operation exceeds limit");
    m.check(dst, n, mem::Write);
    auto bytes = name == "memset" ? std::vector<uint8_t>(n, uint8_t(src)) : m.read(src, n);
    m.write(dst, bytes);
    return dst;
  }
  throw std::logic_error("implemented API has no handler");
}
void Modules::call(cpu::Context &c, mem::AddressSpace &m) {
  auto &api = stubs_.at(c.rip);
  if (trace_)
    log_ << "API " << api.dll << '!' << api.name << '\n';
  uint64_t result;
  try {
    result = invoke(api, c, m);
  } catch (const nt::Error &e) {
    m.store(state_.teb + (c.mode64 ? 0x68 : 0x34), e.code, 4);
    log_ << api.dll << '!' << api.name << ": " << e.what() << " (error " << e.code << ")\n";
    result = (api.name == "GetStdHandle" || api.name == "CreateFileA" ||
              api.name == "CreateFileW" || api.name == "HeapSize")
                 ? UINT64_MAX
                 : 0;
  }
  c.gpr[cpu::RAX] = result & cpu::mask(c.mode64 ? 64 : 32);
  if (state_.exited)
    return;
  auto sp = c.gpr[cpu::RSP];
  auto target = m.load(sp, c.mode64 ? 8 : 4);
  unsigned cleanup = 0;
  if (!c.mode64 && canonical(api.dll) != "msvcrt.dll" && canonical(api.dll) != "ucrtbase.dll") {
    static const std::map<std::string, unsigned> count = {{"GetStdHandle", 1},
                                                          {"WriteFile", 5},
                                                          {"ReadFile", 5},
                                                          {"WriteConsoleA", 5},
                                                          {"WriteConsoleW", 5},
                                                          {"SetLastError", 1},
                                                          {"GetModuleHandleA", 1},
                                                          {"GetModuleHandleW", 1},
                                                          {"CloseHandle", 1},
                                                          {"CreateFileA", 7},
                                                          {"CreateFileW", 7},
                                                          {"SetFilePointerEx", 5},
                                                          {"GetFileSizeEx", 2},
                                                          {"HeapAlloc", 3},
                                                          {"HeapFree", 3},
                                                          {"HeapReAlloc", 4},
                                                          {"HeapSize", 3},
                                                          {"GetEnvironmentVariableA", 3},
                                                          {"GetEnvironmentVariableW", 3},
                                                          {"GetCurrentDirectoryA", 2},
                                                          {"GetCurrentDirectoryW", 2},
                                                          {"QueryPerformanceCounter", 1},
                                                          {"QueryPerformanceFrequency", 1},
                                                          {"MessageBoxW", 4},
                                                          {"MessageBoxA", 4},
                                                          {"GetMessageW", 4},
                                                          {"GetMessageA", 4},
                                                          {"CreateWindowExW", 12},
                                                          {"CreateWindowExA", 12},
                                                          {"Sleep", 1}};
    auto it = count.find(api.name);
    if (it != count.end())
      cleanup = it->second * 4;
  }
  c.gpr[cpu::RSP] = sp + (c.mode64 ? 8 : 4) + cleanup;
  c.rip = target;
}
} // namespace execon::win32
