#include "engine/jit/backend.h"
#include "engine/jit/encoder.h"
#include "engine/x86/decoder.h"
#include "tests/unit/test.h"
using namespace execon;
TEST(arm64_encoder_known_words) {
  jit::Encoder e;
  e.immediate(0, 42);
  e.load(1, 19, 8, 64);
  e.store(2, 19, 16, 64);
  e.alu(0, 3, 1, 2, true);
  CHECK(e.words == std::vector<uint32_t>({0xd2800540, 0xf9400661, 0xf9000a62, 0x8b020023}));
  THROWS(e.load(0, 0, 1, 64));
  THROWS(e.immediate(32, 0));
}
TEST(jit_native_arithmetic_and_cache_invalidation) {
  if (!host::JitMemory::supported())
    return;
  mem::AddressSpace m;
  m.map(0x1000, 4096, 7);
  std::array<uint8_t, 12> code = {0x48, 0xb8, 0x7f, 0, 0, 0, 0, 0, 0, 0, 0x04, 0x01};
  m.write(0x1000, code);
  jit::Dispatcher d;
  cpu::Context c;
  c.rip = 0x1000;
  d.step(c, m);
  CHECK(c.gpr[0] == 0x80);
  CHECK(c.flags() & cpu::OF);
  CHECK(d.stats.translated_blocks == 1);
  c.rip = 0x1000;
  d.step(c, m);
  CHECK(d.stats.cache_hits == 1);
  m.store(0x1002, 0x10, 1);
  c.rip = 0x1000;
  d.step(c, m);
  CHECK(c.gpr[0] == 0x11);
  CHECK(d.stats.invalidations == 1);
}
TEST(jit_memory_fallback_and_precise_fault) {
  mem::AddressSpace m;
  m.map(0x1000, 4096, 7);
  std::array<uint8_t, 3> code = {0x48, 0x8b, 0x00};
  m.write(0x1000, code);
  jit::Dispatcher d(true);
  cpu::Context c;
  c.rip = 0x1000;
  THROWS(d.step(c, m));
  CHECK(c.rip == 0x1000);
  CHECK(d.stats.interpreter_fallbacks == 1);
}
TEST(jit_block_matches_interpreter_and_maps_pc) {
  if (!host::JitMemory::supported())
    return;
  std::vector<uint8_t> code = {0x48, 0x01, 0xd8, 0x48, 0xff, 0xc0, 0x48, 0xf7,
                               0xd8, 0x66, 0x31, 0xd8, 0x48, 0xff, 0xc8};
  ir::Block b{0x1000, {}};
  for (size_t n = 0; n < code.size();) {
    auto i = x86::decode(std::span(code).subspan(n), 0x1000 + n, x86::Mode::X64);
    n += i.length;
    b.instructions.push_back(i);
  }
  host::JitMemory arena;
  auto compiled = jit::compile(b, arena);
  CHECK(compiled.has_value());
  mem::AddressSpace m;
  for (uint64_t n = 0; n < 1000; ++n) {
    cpu::Context a, c;
    a.gpr[0] = n * 0x102030405060708ULL;
    a.gpr[3] = ~n;
    a.rflags = 0x803;
    c = a;
    for (auto &i : b.instructions)
      ir::execute(i, a, m);
    compiled->run(c);
    CHECK(a.gpr == c.gpr);
    CHECK(a.flags() == c.flags());
    CHECK(a.rip == c.rip);
  }
  CHECK(compiled->pc_map.size() == b.instructions.size());
  CHECK(compiled->pc_map.front().second == 0x1000);
}
TEST(self_modifying_write_ends_interpreter_block) {
  mem::AddressSpace m;
  m.map(0x1000, 4096, 7);
  // mov byte [rip+0],0xcc; nop => the replacement int3 must be re-decoded.
  std::array<uint8_t, 8> code = {0xc6, 0x05, 0, 0, 0, 0, 0xcc, 0x90};
  m.write(0x1000, code);
  jit::Dispatcher d(true);
  cpu::Context c;
  c.rip = 0x1000;
  CHECK(d.step(c, m) == 1);
  CHECK(c.rip == 0x1007);
  THROWS(d.step(c, m));
  CHECK(c.rip == 0x1007);
}
