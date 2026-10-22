#include "engine/jit/encoder.h"
#include <stdexcept>
namespace execon::jit {
namespace {
void reg(unsigned r) {
  if (r > 31)
    throw std::logic_error("ARM register out of range");
}
uint32_t transfer(unsigned rt, unsigned rn, unsigned offset, unsigned width, bool load) {
  reg(rt);
  reg(rn);
  unsigned shift;
  switch (width) {
  case 8:
    shift = 0;
    break;
  case 16:
    shift = 1;
    break;
  case 32:
    shift = 2;
    break;
  case 64:
    shift = 3;
    break;
  default:
    throw std::logic_error("invalid ARM transfer width");
  }
  if (offset % (1U << shift) || (offset >> shift) > 4095)
    throw std::logic_error("ARM transfer offset out of range");
  return 0x39000000U | (shift << 30) | (load ? 0x00400000 : 0) | ((offset >> shift) << 10) |
         (rn << 5) | rt;
}
} // namespace
void Encoder::immediate(unsigned dst, uint64_t v) {
  reg(dst);
  emit(0xd2800000U | (uint32_t(v & 0xffff) << 5) | dst);
  for (unsigned n = 1; n < 4; ++n)
    if ((v >> (n * 16)) & 0xffff)
      emit(0xf2800000U | (n << 21) | (uint32_t(v >> (n * 16) & 0xffff) << 5) | dst);
}
void Encoder::load(unsigned dst, unsigned base, unsigned off, unsigned width) {
  emit(transfer(dst, base, off, width, true));
}
void Encoder::store(unsigned src, unsigned base, unsigned off, unsigned width) {
  emit(transfer(src, base, off, width, false));
}
void Encoder::alu(unsigned op, unsigned dst, unsigned lhs, unsigned rhs, bool wide) {
  reg(dst);
  reg(lhs);
  reg(rhs);
  static constexpr uint32_t codes[] = {0x0b000000, 0x4b000000, 0x0a000000, 0x2a000000, 0x4a000000};
  if (op > 4)
    throw std::logic_error("invalid ARM ALU operation");
  emit(codes[op] | (wide ? 0x80000000U : 0) | (rhs << 16) | (lhs << 5) | dst);
}
} // namespace execon::jit
