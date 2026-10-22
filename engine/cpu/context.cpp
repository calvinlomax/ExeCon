#include "engine/cpu/context.h"
#include <bit>
namespace execon::cpu {
uint64_t Context::flags() {
  if (lazy.op == FlagOp::None)
    return rflags;
  auto [op, width, a, b, r] = lazy;
  uint64_t m = mask(width), sign = 1ULL << (width - 1);
  a &= m;
  b &= m;
  r &= m;
  uint64_t f = 0;
  if (!r)
    f |= ZF;
  if (r & sign)
    f |= SF;
  if (std::popcount(uint8_t(r)) % 2 == 0)
    f |= PF;
  if (op == FlagOp::Add || op == FlagOp::Inc) {
    if (r < a)
      f |= CF;
    if ((~(a ^ b) & (a ^ r) & sign))
      f |= OF;
  }
  if (op == FlagOp::Sub || op == FlagOp::Dec) {
    if (a < b)
      f |= CF;
    if (((a ^ b) & (a ^ r) & sign))
      f |= OF;
  }
  if (op != FlagOp::Logic && ((a ^ b ^ r) & 16))
    f |= AF;
  if (op == FlagOp::Inc || op == FlagOp::Dec)
    f = (f & ~CF) | (rflags & CF);
  rflags = (rflags & ~arithmetic_flags) | f | 2;
  lazy.op = FlagOp::None;
  return rflags;
}
void Context::defer(FlagOp op, unsigned width, uint64_t a, uint64_t b, uint64_t r) {
  if (op == FlagOp::Inc || op == FlagOp::Dec)
    flags();
  lazy = {op, uint8_t(width), a, b, r};
}
bool condition(uint8_t cc, uint64_t f) {
  bool of = f & OF, cf = f & CF, zf = f & ZF, sf = f & SF, pf = f & PF;
  switch (cc & 15) {
  case 0:
    return of;
  case 1:
    return !of;
  case 2:
    return cf;
  case 3:
    return !cf;
  case 4:
    return zf;
  case 5:
    return !zf;
  case 6:
    return cf || zf;
  case 7:
    return !cf && !zf;
  case 8:
    return sf;
  case 9:
    return !sf;
  case 10:
    return pf;
  case 11:
    return !pf;
  case 12:
    return sf != of;
  case 13:
    return sf == of;
  case 14:
    return zf || sf != of;
  case 15:
    return !zf && sf == of;
  }
  return false;
}
} // namespace execon::cpu
