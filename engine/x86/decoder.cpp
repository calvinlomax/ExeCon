#include "engine/x86/decoder.h"
#include "engine/cpu/fault.h"
#include <algorithm>
#include <bit>
#include <functional>
#include <iomanip>
#include <sstream>
namespace execon::x86 {
namespace {
using namespace ir;
struct Decoder {
  std::function<uint8_t(unsigned)> fetch;
  Mode mode;
  Instruction ins{};
  uint8_t rex = 0, rep = 0;
  bool op16 = false, addr_override = false, lock = false;
  Segment segment = Segment::Flat;
  [[noreturn]] void bad(const std::string &why) const {
    std::ostringstream out;
    out << "#UD at 0x" << std::hex << ins.pc << " bytes=";
    for (unsigned i = 0; i < ins.length; ++i)
      out << std::setw(2) << std::setfill('0') << unsigned(ins.bytes[i]) << ' ';
    out << "(" << why << ")";
    throw Fault(illegal_instruction, ins.pc, out.str());
  }
  uint8_t byte() {
    if (ins.length == 15)
      bad("instruction exceeds 15 bytes");
    uint8_t b = fetch(ins.length);
    ins.bytes[ins.length++] = b;
    return b;
  }
  uint64_t imm(unsigned n) {
    uint64_t v = 0;
    for (unsigned i = 0; i < n; ++i)
      v |= uint64_t(byte()) << (i * 8);
    return v;
  }
  uint64_t signed_imm(unsigned n) {
    auto v = imm(n);
    if (n == 8)
      return v;
    auto sign = 1ULL << (n * 8 - 1);
    return (v ^ sign) - sign;
  }
  unsigned width() const { return rex & 8 ? 64 : (op16 ? 16 : 32); }
  unsigned stack_width() const { return op16 ? 16 : (mode == Mode::X64 ? 64 : 32); }
  unsigned address_width() const {
    return mode == Mode::X64 ? (addr_override ? 32 : 64) : (addr_override ? 16 : 32);
  }
  Operand reg(unsigned r, unsigned w) const {
    Operand o;
    o.kind = Kind::Register;
    o.width = w;
    o.reg = r;
    if (w == 8 && !rex && r >= 4 && r < 8) {
      o.reg = r - 4;
      o.high8 = true;
    }
    return o;
  }
  Operand immediate(uint64_t v, unsigned w) const {
    Operand o;
    o.kind = Kind::Immediate;
    o.width = w;
    o.immediate = v;
    return o;
  }
  Operand rm(uint8_t m, unsigned w) {
    unsigned mod = m >> 6, r = m & 7;
    if (mod == 3)
      return reg(r + ((rex & 1) ? 8 : 0), w);
    Operand o;
    o.kind = Kind::Memory;
    o.width = w;
    o.address_width = address_width();
    o.segment = segment;
    if (o.address_width == 16)
      bad("16-bit addressing is not implemented");
    if (r == 4) {
      uint8_t s = byte();
      unsigned ix = (s >> 3) & 7, base = s & 7;
      o.scale = 1 << (s >> 6);
      if (ix != 4 || (rex & 2))
        o.index = ix + ((rex & 2) ? 8 : 0);
      if (base == 5 && mod == 0)
        o.displacement = std::bit_cast<int64_t>(signed_imm(4));
      else
        o.base = base + ((rex & 1) ? 8 : 0);
    } else if (r == 5 && mod == 0) {
      o.displacement = std::bit_cast<int64_t>(signed_imm(4));
      o.rip_relative = mode == Mode::X64;
    } else
      o.base = r + ((rex & 1) ? 8 : 0);
    if (mod == 1)
      o.displacement = std::bit_cast<int64_t>(signed_imm(1));
    if (mod == 2)
      o.displacement = std::bit_cast<int64_t>(signed_imm(4));
    return o;
  }
  void pair(unsigned w, bool reverse) {
    auto m = byte();
    auto r = reg(((m >> 3) & 7) + ((rex & 4) ? 8 : 0), w);
    auto mem = rm(m, w);
    ins.dst = reverse ? r : mem;
    ins.src = reverse ? mem : r;
  }
  void relative(Op op, unsigned n) {
    ins.op = op;
    auto d = signed_imm(n);
    ins.dst = immediate((ins.pc + ins.length + d) & cpu::mask(mode == Mode::X64 ? 64 : 32),
                        mode == Mode::X64 ? 64 : 32);
  }
  Instruction run() {
    uint8_t opcode = 0;
    while (true) {
      auto b = byte();
      bool prefix = true;
      switch (b) {
      case 0x66:
        op16 = true;
        rex = 0;
        break;
      case 0x67:
        addr_override = true;
        rex = 0;
        break;
      case 0xf0:
        lock = true;
        rex = 0;
        break;
      case 0xf2:
      case 0xf3:
        rep = b;
        rex = 0;
        break;
      case 0x64:
        segment = Segment::FS;
        rex = 0;
        break;
      case 0x65:
        segment = Segment::GS;
        rex = 0;
        break;
      case 0x26:
      case 0x2e:
      case 0x36:
      case 0x3e:
        segment = Segment::Flat;
        rex = 0;
        break;
      default:
        if (mode == Mode::X64 && b >= 0x40 && b <= 0x4f)
          rex = b;
        else
          prefix = false;
      }
      if (!prefix) {
        opcode = b;
        break;
      }
    }
    if (lock)
      bad("LOCK/atomic operations not implemented");
    if (rep && opcode != 0x90)
      bad("REP/string or mandatory-prefix operation not implemented");
    unsigned w = width();
    // Arithmetic rows share the Intel primary opcode layout; ADC/SBB intentionally absent.
    static constexpr std::array<Op, 8> alu{Op::Add, Op::Or,  Op::Nop, Op::Nop,
                                           Op::And, Op::Sub, Op::Xor, Op::Cmp};
    if (opcode <= 0x3d && (opcode & 7) <= 5) {
      auto group = opcode >> 3;
      if (group == 2 || group == 3)
        bad("ADC/SBB not implemented");
      ins.op = alu[group];
      unsigned form = opcode & 7, bits = (form % 2) ? w : 8;
      if (form < 4)
        pair(bits, form & 2);
      else {
        ins.dst = reg(0, bits);
        ins.src = immediate(bits == 64 ? signed_imm(4) : imm(bits / 8), bits);
      }
    } else if (opcode >= 0xb0 && opcode <= 0xbf) {
      unsigned bits = opcode < 0xb8 ? 8 : w;
      ins.op = Op::Mov;
      ins.dst = reg((opcode & 7) + ((rex & 1) ? 8 : 0), bits);
      ins.src = immediate(imm(bits / 8), bits);
    } else if (opcode >= 0x50 && opcode <= 0x5f) {
      ins.op = opcode < 0x58 ? Op::Push : Op::Pop;
      ins.dst = reg((opcode & 7) + ((rex & 1) ? 8 : 0), stack_width());
    } else if (opcode >= 0x70 && opcode <= 0x7f) {
      ins.condition = opcode & 15;
      relative(Op::Branch, 1);
    } else if (mode == Mode::X86 && opcode >= 0x40 && opcode <= 0x4f) {
      ins.op = opcode < 0x48 ? Op::Inc : Op::Dec;
      ins.dst = reg(opcode & 7, w);
    } else
      switch (opcode) {
      case 0x90:
        ins.op = Op::Nop;
        if (rex & 1)
          bad("XCHG not implemented");
        break;
      case 0x88:
      case 0x89:
      case 0x8a:
      case 0x8b:
        ins.op = Op::Mov;
        pair(opcode & 1 ? w : 8, opcode & 2);
        break;
      case 0x8d:
        ins.op = Op::Lea;
        pair(w, true);
        if (ins.src.kind != Kind::Memory)
          bad("LEA requires memory encoding");
        break;
      case 0x84:
      case 0x85:
        ins.op = Op::Test;
        pair(opcode & 1 ? w : 8, false);
        break;
      case 0xa0:
      case 0xa1:
      case 0xa2:
      case 0xa3: {
        if (address_width() == 16)
          bad("16-bit addresses not implemented");
        Operand memory;
        memory.kind = Kind::Memory;
        memory.width = (opcode & 1) ? w : 8;
        memory.address_width = address_width();
        memory.segment = segment;
        memory.displacement = std::bit_cast<int64_t>(imm(address_width() / 8));
        ins.op = Op::Mov;
        if (opcode & 2) {
          ins.dst = memory;
          ins.src = reg(0, memory.width);
        } else {
          ins.dst = reg(0, memory.width);
          ins.src = memory;
        }
        break;
      }
      case 0xa8:
      case 0xa9:
        ins.op = Op::Test;
        ins.dst = reg(0, opcode & 1 ? w : 8);
        ins.src = immediate(w == 64 && (opcode & 1) ? signed_imm(4) : imm(ins.dst.width / 8),
                            ins.dst.width);
        break;
      case 0xc6:
      case 0xc7: {
        auto m = byte();
        if ((m >> 3 & 7) != 0)
          bad("unsupported MOV group");
        ins.op = Op::Mov;
        unsigned bits = opcode == 0xc6 ? 8 : w;
        ins.dst = rm(m, bits);
        ins.src = immediate(bits == 64 ? signed_imm(4) : imm(bits / 8), bits);
        break;
      }
      case 0x80:
      case 0x81:
      case 0x83: {
        auto m = byte();
        unsigned group = m >> 3 & 7;
        if (group == 2 || group == 3)
          bad("ADC/SBB not implemented");
        ins.op = alu[group];
        unsigned bits = opcode == 0x80 ? 8 : w;
        ins.dst = rm(m, bits);
        ins.src = immediate(
            opcode == 0x83 ? signed_imm(1) : (bits == 64 ? signed_imm(4) : imm(bits / 8)), bits);
        break;
      }
      case 0xf6:
      case 0xf7: {
        auto m = byte();
        unsigned g = m >> 3 & 7, bits = opcode == 0xf6 ? 8 : w;
        ins.dst = rm(m, bits);
        if (g == 0) {
          ins.op = Op::Test;
          ins.src = immediate(bits == 64 ? signed_imm(4) : imm(bits / 8), bits);
        } else if (g == 2)
          ins.op = Op::Not;
        else if (g == 3)
          ins.op = Op::Neg;
        else
          bad("MUL/DIV group not implemented");
        break;
      }
      case 0xfe:
      case 0xff: {
        auto m = byte();
        unsigned g = m >> 3 & 7, bits = opcode == 0xfe ? 8 : w;
        if (opcode == 0xff && (g == 2 || g == 4 || g == 6))
          bits = stack_width();
        ins.dst = rm(m, bits);
        if (g == 0)
          ins.op = Op::Inc;
        else if (g == 1)
          ins.op = Op::Dec;
        else if (opcode == 0xff && g == 2)
          ins.op = Op::Call;
        else if (opcode == 0xff && g == 4)
          ins.op = Op::Jump;
        else if (opcode == 0xff && g == 6)
          ins.op = Op::Push;
        else
          bad("unsupported FF group");
        break;
      }
      case 0x68:
      case 0x6a:
        ins.op = Op::Push;
        ins.dst = immediate(signed_imm(opcode == 0x6a ? 1 : (op16 ? 2 : 4)), stack_width());
        break;
      case 0xe8:
      case 0xe9:
        if (op16)
          bad("16-bit relative CALL/JMP not implemented");
        relative(opcode == 0xe8 ? Op::Call : Op::Jump, 4);
        break;
      case 0xeb:
        relative(Op::Jump, 1);
        break;
      case 0xc3:
      case 0xc2:
        if (op16)
          bad("16-bit RET not implemented");
        ins.op = Op::Ret;
        if (opcode == 0xc2)
          ins.src = immediate(imm(2), 16);
        break;
      case 0xc0:
      case 0xc1:
      case 0xd0:
      case 0xd1:
      case 0xd2:
      case 0xd3: {
        auto m = byte();
        unsigned g = m >> 3 & 7;
        ins.dst = rm(m, opcode & 1 ? w : 8);
        if (g == 4)
          ins.op = Op::Shl;
        else if (g == 5)
          ins.op = Op::Shr;
        else if (g == 7)
          ins.op = Op::Sar;
        else
          bad("rotate group not implemented");
        if (opcode == 0xc0 || opcode == 0xc1)
          ins.src = immediate(imm(1), 8);
        else if (opcode == 0xd0 || opcode == 0xd1)
          ins.src = immediate(1, 8);
        else
          ins.src = reg(1, 8);
        break;
      }
      case 0x63:
        if (mode != Mode::X64 || !(rex & 8))
          bad("MOVSXD form not implemented");
        ins.op = Op::Movsx;
        pair(32, true);
        ins.dst.width = 64;
        break;
      case 0xfc:
        ins.op = Op::Cld;
        break;
      case 0xfd:
        ins.op = Op::Std;
        break;
      case 0xcc:
        ins.op = Op::Breakpoint;
        break;
      case 0x0f: {
        auto op = byte();
        if (op >= 0x80 && op <= 0x8f) {
          if (op16)
            bad("16-bit Jcc not implemented");
          ins.condition = op & 15;
          relative(Op::Branch, 4);
        } else if (op >= 0x90 && op <= 0x9f) {
          auto m = byte();
          ins.op = Op::Setcc;
          ins.condition = op & 15;
          ins.dst = rm(m, 8);
        } else if (op >= 0x40 && op <= 0x4f) {
          ins.op = Op::Cmov;
          ins.condition = op & 15;
          pair(w, true);
        } else if (op == 0xb6 || op == 0xb7 || op == 0xbe || op == 0xbf) {
          ins.op = op & 8 ? Op::Movsx : Op::Movzx;
          auto m = byte();
          ins.dst = reg((m >> 3 & 7) + ((rex & 4) ? 8 : 0), w);
          ins.src = rm(m, op & 1 ? 16 : 8);
        } else if (op == 0x1f) {
          auto m = byte();
          if ((m >> 3 & 7) != 0)
            bad("invalid NOP group");
          (void)rm(m, w);
          ins.op = Op::Nop;
        } else if (op == 0xa2)
          ins.op = Op::Cpuid;
        else if (op == 0x31)
          ins.op = Op::Rdtsc;
        else
          bad("unsupported 0F opcode");
        break;
      }
      default:
        bad("unsupported primary opcode");
      }
    return ins;
  }
};
} // namespace
ir::Instruction decode(std::span<const uint8_t> bytes, uint64_t pc, Mode mode) {
  Decoder d{[&](unsigned i) {
              if (i >= bytes.size())
                throw Fault(illegal_instruction, pc,
                            "#UD: truncated instruction at " + std::to_string(pc));
              return bytes[i];
            },
            mode};
  d.ins.pc = pc;
  return d.run();
}
ir::Instruction decode(const mem::AddressSpace &m, uint64_t pc, Mode mode) {
  Decoder d{[&](unsigned i) { return uint8_t(m.load(pc + i, 1, mem::Execute)); }, mode};
  d.ins.pc = pc;
  return d.run();
}
ir::Block block(const mem::AddressSpace &m, uint64_t pc, Mode mode, unsigned max) {
  ir::Block b{pc, {}};
  for (unsigned i = 0; i < max; ++i) {
    try {
      auto ins = decode(m, pc, mode);
      b.instructions.push_back(ins);
      pc += ins.length;
      if (ins.terminates())
        break;
    } catch (const Fault &) {
      if (b.instructions.empty())
        throw;
      break;
    }
  }
  return b;
}
std::string coverage() {
  return "Hand-written primary ALU table: ADD OR AND SUB XOR CMP; MOV/MOVZX/MOVSX/LEA; TEST INC "
         "DEC NEG NOT; PUSH POP CALL JMP Jcc RET; SETcc CMOVcc; SHL SHR SAR; NOP CLD STD CPUID "
         "RDTSC INT3.\n32/64-bit modes; 8/16/32/64-bit operands; "
         "REX/ModRM/SIB/RIP/FS/GS.\nUnsupported: ADC/SBB, rotate, MUL/DIV, REP strings, LOCK "
         "atomics, SSE*, AVX, x87, 16-bit addresses. Unsupported forms raise #UD.\n";
}
} // namespace execon::x86
