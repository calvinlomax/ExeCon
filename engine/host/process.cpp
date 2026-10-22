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
void unicode(mem::AddressSpace &m, uint64_t descriptor, uint64_t buffer, const std::u16string &s) {
  if (s.size() > 32766)
    throw std::runtime_error("process string exceeds UNICODE_STRING limit");
  string_w(m, buffer, s);
  m.store(descriptor, s.size() * 2, 2);
  m.store(descriptor + 2, s.size() * 2 + 2, 2);
  m.store(descriptor + 8, buffer, 8);
}
} // namespace
uint32_t run(const pe::Image &p, const std::vector<std::string> &args, const Options &o,
             std::ostream &log) {
  if (!p.is64)
    throw std::runtime_error("PE32 execution is not implemented; --inspect supports PE32");
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
  mem::AddressSpace m;
  uint64_t base = p.load(m);
  constexpr uint64_t stack = 0x10000000, stack_size = 0x100000, teb = 0x7ff00000, peb = 0x7ff01000,
                     params = 0x7ff02000, ldr = 0x7ff03000, strings = 0x7ff10000;
  m.map(stack, stack_size, mem::Read | mem::Write);
  m.map(teb, 0x4000, mem::Read | mem::Write);
  m.map(strings, 0x80000, mem::Read | mem::Write);
  cpu::Context c;
  c.rip = base + p.entry;
  c.gpr[cpu::RSP] = stack + stack_size - 40;
  c.gs_base = teb;
  m.store(c.gpr[cpu::RSP], 0, 8);
  m.store(teb + 8, stack + stack_size, 8);
  m.store(teb + 16, stack, 8);
  m.store(teb + 0x30, teb, 8);
  m.store(teb + 0x60, peb, 8);
  m.store(teb + 0x48, 1, 8);
  m.store(peb + 0x10, base, 8);
  m.store(peb + 0x18, ldr, 8);
  m.store(peb + 0x20, params, 8);
  m.store(ldr, 0x58, 4);
  m.store(ldr + 4, 1, 1);
  // Circular loader lists contain the main image. DLL modules are synthetic thunks, not PE modules.
  uint64_t entry = ldr + 0x100;
  for (unsigned n = 0; n < 3; ++n) {
    auto head = ldr + 0x10 + n * 0x10, node = entry + n * 0x10;
    m.store(head, node, 8);
    m.store(head + 8, node, 8);
    m.store(node, head, 8);
    m.store(node + 8, head, 8);
  }
  m.store(entry + 0x30, base, 8);
  m.store(entry + 0x38, base + p.entry, 8);
  m.store(entry + 0x40, p.image_size, 4);
  auto command = nt::command_line(args);
  auto cmd16 = nt::utf16(command);
  if (command.size() > 65535)
    throw std::runtime_error("command line too long");
  string_a(m, strings, command);
  unicode(m, params + 0x70, strings + 0x10000, cmd16);
  unicode(m, params + 0x60, strings + 0x20000, nt::utf16(args.at(0)));
  unicode(m, entry + 0x48, strings + 0x20000, nt::utf16(args.at(0)));
  unicode(m, entry + 0x58, strings + 0x30000,
          nt::utf16(std::filesystem::path(args.at(0)).filename().string()));
  unicode(m, params + 0x38, strings + 0x40000, nt::utf16(o.cwd.empty() ? "C:\\" : o.cwd.string()));
  m.store(params, 0x1000, 4);
  m.store(params + 4, 0x1000, 4);
  m.store(params + 8, 1, 4);
  nt::Services services;
  m.store(teb + 0x40, services.process_id(), 8);
  for (unsigned n = 0; n < 3; ++n)
    m.store(params + 0x20 + n * 8, services.standard_handle(-10 - int(n)), 8);
  uint64_t env = strings + 0x50000, cursor = env;
  for (auto &[key, value] : o.environment) {
    auto text = nt::utf16(key + "=" + value);
    if (text.size() * 2 + 2 > strings + 0x80000 - cursor - 2)
      throw std::runtime_error("environment too large");
    string_w(m, cursor, text);
    cursor += text.size() * 2 + 2;
  }
  m.store(cursor, 0, 2);
  m.store(params + 0x80, env, 8);
  win32::ProcessState state{base, teb, strings, strings + 0x10000, false, 0};
  win32::Modules modules(services, state, log, o.missing, o.trace_api);
  modules.bind(p, base, m);
  jit::Dispatcher dispatcher(o.interp_only);
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
