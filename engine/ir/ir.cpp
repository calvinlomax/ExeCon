#include "engine/ir/ir.h"
#include "engine/cpu/fault.h"
#include <bit>
#include <iomanip>
#include <sstream>
#include <stdexcept>
namespace execon::ir {
bool Instruction::writes_flags() const {
  return (op >= Op::Add && op <= Op::Neg) || (op >= Op::Shl && op <= Op::Sar);
}
bool Instruction::terminates() const {
  return op == Op::Call || op == Op::Jump || op == Op::Branch || op == Op::Ret ||
         op == Op::Breakpoint || op == Op::HostCall;
}
uint64_t address(const Operand &o, const cpu::Context &c, uint64_t next, bool segment) {
  uint64_t v = uint64_t(o.displacement);
  if (o.rip_relative)
    v += next;
  if (o.base >= 0)
    v += c.gpr[o.base];
  if (o.index >= 0)
    v += c.gpr[o.index] * o.scale;
  v &= cpu::mask(o.address_width);
  if (segment) {
    if (o.segment == Segment::FS)
      v += c.fs_base;
    if (o.segment == Segment::GS)
      v += c.gs_base;
  }
  return v;
}
uint64_t read(const Operand &o, cpu::Context &c, mem::AddressSpace &m, uint64_t next) {
  switch (o.kind) {
  case Kind::Register:
    return (c.gpr[o.reg] >> (o.high8 ? 8 : 0)) & cpu::mask(o.width);
  case Kind::Immediate:
    return o.immediate & cpu::mask(o.width);
  case Kind::Memory:
    return m.load(address(o, c, next), o.width / 8);
  case Kind::None:
    return 0;
  }
  throw std::logic_error("invalid operand");
}
void write(const Operand &o, cpu::Context &c, mem::AddressSpace &m, uint64_t next, uint64_t v) {
  v &= cpu::mask(o.width);
  if (o.kind == Kind::Register) {
    auto &r = c.gpr[o.reg];
    if (o.width >= 32)
      r = v;
    else {
      auto shift = o.high8 ? 8 : 0;
      r = (r & ~(cpu::mask(o.width) << shift)) | (v << shift);
    }
  } else if (o.kind == Kind::Memory)
    m.store(address(o, c, next), v, o.width / 8);
  else
    throw std::logic_error("non-writable operand");
}
void execute(const Instruction &i, cpu::Context &c, mem::AddressSpace &m) {
  using namespace cpu;
  uint64_t next = (i.pc + i.length) & mask(c.mode64 ? 64 : 32), a = 0, b = 0, r = 0;
  auto width = i.dst.width;
  auto dst = [&] { return read(i.dst, c, m, next); };
  auto src = [&] { return read(i.src, c, m, next); };
  auto store = [&](uint64_t value) { write(i.dst, c, m, next, value); };
  auto push = [&](uint64_t value, unsigned n) {
    auto sp = (c.gpr[RSP] - n) & mask(c.mode64 ? 64 : 32);
    m.store(sp, value, n);
    c.gpr[RSP] = sp;
  };
  FlagOp f = FlagOp::None;
  switch (i.op) {
  case Op::Nop:
    break;
  case Op::Mov:
  case Op::Movzx:
    store(src());
    break;
  case Op::Movsx: {
    auto v = src(), sign = 1ULL << (i.src.width - 1);
    store((v ^ sign) - sign);
    break;
  }
  case Op::Lea:
    store(address(i.src, c, next, false));
    break;
  case Op::Add:
  case Op::Sub:
  case Op::And:
  case Op::Or:
  case Op::Xor:
  case Op::Cmp:
  case Op::Test:
    a = dst();
    b = src();
    if (i.op == Op::Add) {
      r = a + b;
      f = FlagOp::Add;
    } else if (i.op == Op::Sub || i.op == Op::Cmp) {
      r = a - b;
      f = FlagOp::Sub;
    } else {
      r = i.op == Op::Or ? (a | b) : i.op == Op::Xor ? (a ^ b) : (a & b);
      f = FlagOp::Logic;
    }
    if (i.op != Op::Cmp && i.op != Op::Test)
      store(r);
    break;
  case Op::Inc:
  case Op::Dec:
    a = dst();
    b = 1;
    r = i.op == Op::Inc ? a + 1 : a - 1;
    store(r);
    f = i.op == Op::Inc ? FlagOp::Inc : FlagOp::Dec;
    break;
  case Op::Neg:
    b = dst();
    a = 0;
    r = 0 - b;
    store(r);
    f = FlagOp::Sub;
    break;
  case Op::Not:
    store(~dst());
    break;
  case Op::Push:
    push(dst(), width / 8);
    break;
  case Op::Pop: {
    auto sp = c.gpr[RSP];
    auto v = m.load(sp, width / 8);
    if (i.dst.kind != Kind::Register)
      throw std::logic_error("memory POP not supported");
    c.gpr[RSP] = (sp + width / 8) & mask(c.mode64 ? 64 : 32);
    store(v);
    break;
  }
  case Op::Call: {
    auto target = dst();
    push(next, c.mode64 ? 8 : 4);
    next = target;
    break;
  }
  case Op::Jump:
    next = dst();
    break;
  case Op::Branch:
    if (condition(i.condition, c.flags()))
      next = dst();
    break;
  case Op::Ret: {
    auto sp = c.gpr[RSP];
    next = m.load(sp, c.mode64 ? 8 : 4);
    c.gpr[RSP] = (sp + (c.mode64 ? 8 : 4) + src()) & mask(c.mode64 ? 64 : 32);
    break;
  }
  case Op::Setcc:
    store(condition(i.condition, c.flags()));
    break;
  case Op::Cmov: {
    auto v = src();
    if (condition(i.condition, c.flags()))
      store(v);
    else if (width == 32)
      store(dst());
    break;
  }
  case Op::Shl:
  case Op::Shr:
  case Op::Sar: {
    a = dst();
    unsigned count = src() & (width == 64 ? 63 : 31);
    if (!count) {
      if (i.dst.kind == Kind::Register && width == 32)
        store(a);
      break;
    }
    uint64_t flags = c.flags();
    bool carry = false;
    r = a;
    for (unsigned n = 0; n < count; ++n) {
      if (i.op == Op::Shl) {
        carry = r & (1ULL << (width - 1));
        r = (r << 1) & mask(width);
      } else {
        carry = r & 1;
        r = (r >> 1) | ((i.op == Op::Sar) ? (r & (1ULL << (width - 1))) : 0);
      }
    }
    store(r);
    c.defer(FlagOp::Logic, width, 0, 0, r);
    uint64_t value = c.flags();
    value = (value & ~(CF | OF | AF)) | (flags & (OF | AF));
    if (carry)
      value |= CF;
    if (count == 1) {
      value &= ~OF;
      bool of = i.op == Op::Shl   ? bool((r >> (width - 1)) ^ carry)
                : i.op == Op::Shr ? bool(a >> (width - 1))
                                  : false;
      if (of)
        value |= OF;
    }
    c.rflags = value;
    break;
  }
  case Op::Cld:
    c.flags();
    c.rflags &= ~DF;
    break;
  case Op::Std:
    c.flags();
    c.rflags |= DF;
    break;
  case Op::Cpuid:
    c.gpr[RAX] = 0;
    c.gpr[RBX] = 0;
    c.gpr[RCX] = 0;
    c.gpr[RDX] = 0;
    break;
  case Op::Rdtsc:
    throw Fault(illegal_instruction, i.pc, "RDTSC timing model not implemented");
  case Op::Breakpoint:
    throw Fault(0x80000003, i.pc, "guest INT3 breakpoint");
  case Op::HostCall:
    throw std::logic_error("HostCall requires process dispatcher");
  }
  if (f != FlagOp::None)
    c.defer(f, width, a, b, r);
  c.rip = next;
}
void optimize(Block &b) {
  for (auto &i : b.instructions)
    if (i.op == Op::Lea && i.src.base < 0 && i.src.index < 0) {
      auto value = address(i.src, cpu::Context{}, i.pc + i.length, false);
      i.op = Op::Mov;
      i.src = {};
      i.src.kind = Kind::Immediate;
      i.src.width = i.dst.width;
      i.src.immediate = value;
    }
}
std::string op_name(Op op) {
  static constexpr const char *names[] = {
      "nop",  "mov", "movzx", "movsx", "lea",   "add",     "sub", "and",  "or",
      "xor",  "cmp", "test",  "inc",   "dec",   "neg",     "not", "push", "pop",
      "call", "jmp", "jcc",   "ret",   "setcc", "cmovcc",  "shl", "shr",  "sar",
      "cld",  "std", "cpuid", "rdtsc", "int3",  "hostcall"};
  return names[unsigned(op)];
}
std::string format(const Instruction &i) {
  std::ostringstream s;
  s << "0x" << std::hex << i.pc << ": " << op_name(i.op);
  auto operand = [&](const Operand &o) {
    s << " ";
    if (o.kind == Kind::Register)
      s << "r" << std::dec << unsigned(o.reg) << ":" << unsigned(o.width) << (o.high8 ? "h" : "");
    else if (o.kind == Kind::Immediate)
      s << "0x" << std::hex << o.immediate;
    else if (o.kind == Kind::Memory) {
      s << (o.segment == Segment::FS ? "fs:" : o.segment == Segment::GS ? "gs:" : "") << "[";
      if (o.rip_relative)
        s << "rip+";
      if (o.base >= 0)
        s << "r" << std::dec << int(o.base) << "+";
      if (o.index >= 0)
        s << "r" << int(o.index) << "*" << unsigned(o.scale) << "+";
      s << o.displacement << "]:" << unsigned(o.width);
    }
  };
  if (i.dst.kind != Kind::None)
    operand(i.dst);
  if (i.src.kind != Kind::None) {
    s << ",";
    operand(i.src);
  }
  if (i.op == Op::Branch || i.op == Op::Cmov || i.op == Op::Setcc)
    s << " cc=" << unsigned(i.condition);
  return s.str();
}
} // namespace execon::ir
