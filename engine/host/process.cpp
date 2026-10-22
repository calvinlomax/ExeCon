#include "engine/host/process.h"
#include "engine/nt/text.h"
namespace execon {
namespace {
void string_a(mem::AddressSpace &m, uint64_t a, const std::string &s) {
  m.write(a, std::span(reinterpret_cast<const uint8_t *>(s.data()), s.size()));
  m.store(a + s.size(), 0, 1);
}
void string_w(mem::AddressSpace &m, uint64_t a, const std::u16string &s) {
  for (size_t i = 0; i < s.size(); ++i)
    m.store(a + i * 2, s[i], 2);
  m.store(a + s.size() * 2, 0, 2);
}
void unicode(mem::AddressSpace &m, uint64_t descriptor, uint64_t buffer, const std::u16string &s,
             bool x64) {
  if (s.size() > 32766)
    throw std::runtime_error("process string exceeds UNICODE_STRING limit");
  string_w(m, buffer, s);
  m.store(descriptor, s.size() * 2, 2);
  m.store(descriptor + 2, s.size() * 2 + 2, 2);
  m.store(descriptor + (x64 ? 8 : 4), buffer, x64 ? 8 : 4);
}
} // namespace
uint32_t run(const pe::Image &p, const std::vector<std::string> &args, const Options &o,
             std::ostream &log) {
  if (p.subsystem != 3)
    throw std::runtime_error("only Windows console subsystem images are executable");
  if (p.characteristics & 0x2000)
    throw std::runtime_error("a DLL cannot be used as the process executable");
  if (p.directories[9].size)
    throw std::runtime_error("TLS initialization is not implemented");
  if (p.directories[10].size)
    throw std::runtime_error("load-config initialization is not implemented");
  if (p.directories[13].size)
    throw std::runtime_error("delay-load initialization is not implemented");
  if (p.directories[14].size)
    throw std::runtime_error("managed CLR images are not implemented");
  mem::AddressSpace m(!p.is64);
  unsigned pointer = p.is64 ? 8 : 4;
  uint64_t base = p.load(m);
  constexpr uint64_t stack = 0x10000000, stack_size = 0x100000, teb = 0x7ff00000, peb = 0x7ff01000,
                     params = 0x7ff02000, ldr = 0x7ff03000, strings = 0x7ff10000;
  m.map(stack, stack_size, mem::Read | mem::Write);
  m.map(teb, 0x4000, mem::Read | mem::Write);
  m.map(strings, 0x80000, mem::Read | mem::Write);
  cpu::Context c;
  c.mode64 = p.is64;
  c.rip = base + p.entry;
  c.gpr[cpu::RSP] = stack + stack_size - (p.is64 ? 40 : 20);
  if (p.is64)
    c.gs_base = teb;
  else
    c.fs_base = teb;
  m.store(c.gpr[cpu::RSP], 0, pointer);
  if (!p.is64)
    m.store(teb, 0xffffffff, 4); // Empty x86 SEH chain (dispatch remains unsupported).
  m.store(teb + pointer, stack + stack_size, pointer);
  m.store(teb + pointer * 2, stack, pointer);
  m.store(teb + (p.is64 ? 0x30 : 0x18), teb, pointer);
  m.store(teb + (p.is64 ? 0x60 : 0x30), peb, pointer);
  m.store(teb + (p.is64 ? 0x48 : 0x24), 1, pointer);
  m.store(peb + (p.is64 ? 0x10 : 0x08), base, pointer);
  m.store(peb + (p.is64 ? 0x18 : 0x0c), ldr, pointer);
  m.store(peb + (p.is64 ? 0x20 : 0x10), params, pointer);
  m.store(ldr, p.is64 ? 0x58 : 0x30, 4);
  m.store(ldr + 4, 1, 1);
  uint64_t entry = ldr + 0x100;
  for (unsigned n = 0; n < 3; ++n) {
    auto head = ldr + (p.is64 ? 0x10 : 0xc) + n * pointer * 2, node = entry + n * pointer * 2;
    m.store(head, node, pointer);
    m.store(head + pointer, node, pointer);
    m.store(node, head, pointer);
    m.store(node + pointer, head, pointer);
  }
  m.store(entry + (p.is64 ? 0x30 : 0x18), base, pointer);
  m.store(entry + (p.is64 ? 0x38 : 0x1c), base + p.entry, pointer);
  m.store(entry + (p.is64 ? 0x40 : 0x20), p.image_size, 4);
  auto command = nt::command_line(args);
  auto cmd16 = nt::utf16(command);
  if (command.size() > 65535)
    throw std::runtime_error("command line too long");
  string_a(m, strings, command);
  unicode(m, params + (p.is64 ? 0x70 : 0x40), strings + 0x10000, cmd16, p.is64);
  unicode(m, params + (p.is64 ? 0x60 : 0x38), strings + 0x20000, nt::utf16(args.at(0)), p.is64);
  unicode(m, entry + (p.is64 ? 0x48 : 0x24), strings + 0x20000, nt::utf16(args.at(0)), p.is64);
  unicode(m, entry + (p.is64 ? 0x58 : 0x2c), strings + 0x30000,
          nt::utf16(std::filesystem::path(args.at(0)).filename().string()), p.is64);
  unicode(m, params + (p.is64 ? 0x38 : 0x24), strings + 0x40000,
          nt::utf16(o.cwd.empty() ? "C:\\" : o.cwd.string()), p.is64);
  m.store(params, 0x1000, 4);
  m.store(params + 4, 0x1000, 4);
  m.store(params + 8, 1, 4);
  nt::Services services(nt::Paths(o.prefix, o.cwd.empty() ? "C:\\" : o.cwd.string(), o.drives));
  nt::Heap heap(m);
  m.store(teb + (p.is64 ? 0x40 : 0x20), services.process_id(), pointer);
  for (unsigned n = 0; n < 3; ++n)
    m.store(params + (p.is64 ? 0x20 : 0x18) + n * pointer, services.standard_handle(-10 - int(n)),
            pointer);
  uint64_t env = strings + 0x50000, cursor = env;
  for (auto &[key, value] : o.environment) {
    auto text = nt::utf16(key + "=" + value);
    if (text.size() * 2 + 2 > strings + 0x80000 - cursor - 2)
      throw std::runtime_error("environment too large");
    string_w(m, cursor, text);
    cursor += text.size() * 2 + 2;
  }
  m.store(cursor, 0, 2);
  m.store(params + (p.is64 ? 0x80 : 0x48), env, pointer);
  win32::ProcessState state{base, teb, strings, strings + 0x10000, o.environment, false, 0};
  win32::Modules modules(services, heap, state, log, o.missing, o.trace_api);
  modules.bind(p, base, m);
  jit::Dispatcher dispatcher(o.interp_only || !p.is64);
  auto print_stats = [&] {
    if (o.stats) {
      auto &s = dispatcher.stats;
      log << "stats: translated_blocks=" << s.translated_blocks << " cache_hits=" << s.cache_hits
          << " interpreter_fallbacks=" << s.interpreter_fallbacks
          << " invalidations=" << s.invalidations << " instructions=" << s.instructions << '\n';
    }
  };
  uint64_t steps = 0;
  try {
    while (!state.exited) {
      if (++steps > o.instruction_limit || dispatcher.stats.instructions > o.instruction_limit)
        throw std::runtime_error("guest instruction budget exceeded (EXECON_MAX_INSTRUCTIONS)");
      if (modules.contains(c.rip)) {
        modules.call(c, m);
        continue;
      }
      if (c.rip == 0)
        throw std::runtime_error(
            "entry point returned without ExitProcess; CRT startup not provided");
      dispatcher.step(c, m, (o.trace || o.dump_ir || o.dump_asm) ? &log : nullptr, o.dump_ir,
                      o.dump_asm);
    }
  } catch (...) {
    print_stats();
    throw;
  }
  print_stats();
  return state.exit_code;
}
} // namespace execon
