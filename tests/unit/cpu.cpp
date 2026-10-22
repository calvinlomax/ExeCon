#include "engine/x86/decoder.h"
#include "tests/unit/test.h"
#include <initializer_list>
using namespace execon;
static ir::Instruction decode(std::initializer_list<uint8_t> b) {
  return x86::decode(std::span(b.begin(), b.size()), 0x1000, x86::Mode::X64);
}
TEST(decoder_addressing_and_prefixes) {
  auto i = decode({0x65, 0x48, 0x8b, 0x04, 0x25, 0x60, 0, 0, 0});
  CHECK(i.length == 9);
  CHECK(i.src.segment == ir::Segment::GS);
  CHECK(i.src.base == -1);
  cpu::Context c;
  c.gs_base = 0x10000;
  CHECK(ir::address(i.src, c, 0) == 0x10060);
  i = decode({0x48, 0x8d, 0x05, 0xfc, 0xff, 0xff, 0xff});
  CHECK(ir::address(i.src, c, 0x1007) == 0x1003);
  THROWS(decode({0xf0, 0x48, 0x01, 0xd8}));
  THROWS(decode({0x0f, 0x0b}));
  THROWS(decode({0x48, 0xb8, 1}));
  std::array<uint8_t, 16> b{};
  b.fill(0x66);
  THROWS(x86::decode(b, 0, x86::Mode::X64));
}
TEST(flags_edges_and_lazy_carry) {
  cpu::Context c;
  c.defer(cpu::FlagOp::Add, 8, 0x7f, 1, 0x80);
  CHECK((c.flags() & cpu::arithmetic_flags) == (cpu::OF | cpu::SF | cpu::AF));
  c.defer(cpu::FlagOp::Sub, 64, 0, 1, UINT64_MAX);
  CHECK(c.flags() & cpu::CF);
  c.defer(cpu::FlagOp::Inc, 64, 1, 1, 2);
  CHECK(c.flags() & cpu::CF);
  c.defer(cpu::FlagOp::Logic, 32, 0, 0, 0);
  CHECK((c.flags() & cpu::arithmetic_flags) == (cpu::ZF | cpu::PF));
}
TEST(interpreter_fault_is_precise) {
  mem::AddressSpace m;
  cpu::Context c;
  c.rip = 0x1000;
  c.gpr[0] = 0x2000;
  c.gpr[3] = 12;
  c.rflags = 0x803;
  auto i = decode({0x48, 0x01, 0x18});
  THROWS(ir::execute(i, c, m));
  CHECK(c.rip == 0x1000);
  CHECK(c.flags() == 0x803);
  CHECK(c.gpr[0] == 0x2000);
}
TEST(optimizer_preserves_guest_boundary) {
  auto i = decode({0x48, 0x8d, 0x05, 0xfc, 0xff, 0xff, 0xff});
  ir::Block b{0x1000, {i}};
  ir::optimize(b);
  CHECK(b.instructions[0].op == ir::Op::Mov);
  mem::AddressSpace m;
  cpu::Context a, c;
  ir::execute(i, a, m);
  ir::execute(b.instructions[0], c, m);
  CHECK(a.gpr == c.gpr);
  CHECK(a.rip == c.rip);
}
TEST(zero_count_shift_clears_upper_register_without_changing_flags) {
  cpu::Context c;
  mem::AddressSpace m;
  c.gpr[0] = 0x8000000012345678ULL;
  c.gpr[1] = 32;
  c.rflags = 0x803;
  ir::execute(decode({0xd3, 0xe0}), c, m);
  CHECK(c.gpr[0] == 0x12345678);
  CHECK(c.flags() == 0x803);
}
