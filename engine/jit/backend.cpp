#include "engine/jit/backend.h"
#include "engine/jit/encoder.h"
#include "engine/x86/decoder.h"
#include <cstddef>
#include <iomanip>
#include <type_traits>
namespace execon::jit {
namespace {
void defer_flags(cpu::Context *c, uint64_t a, uint64_t b, uint64_t r, uint64_t op, uint64_t width) {
  c->defer(static_cast<cpu::FlagOp>(op), unsigned(width), a, b, r);
}
bool supported(const ir::Instruction &i) {
  using namespace ir;
  if (i.op == Op::Nop)
    return true;
  if (i.dst.kind != Kind::Register)
    return false;
  if (i.src.kind != Kind::Register && i.src.kind != Kind::Immediate && i.src.kind != Kind::None)
    return false;
  switch (i.op) {
  case Op::Mov:
  case Op::Add:
  case Op::Sub:
  case Op::And:
  case Op::Or:
  case Op::Xor:
  case Op::Cmp:
  case Op::Test:
  case Op::Inc:
  case Op::Dec:
  case Op::Neg:
  case Op::Not:
    return true;
  default:
    return false;
  }
}
void operand(Encoder &e, unsigned target, const ir::Operand &o) {
  if (o.kind == ir::Kind::Immediate)
    e.immediate(target, o.immediate & cpu::mask(o.width));
  else
    e.load(target, 19, unsigned(offsetof(cpu::Context, gpr)) + o.reg * 8 + (o.high8 ? 1 : 0),
           o.width);
}
} // namespace
void NativeBlock::run(cpu::Context &c) const {
  reinterpret_cast<void (*)(cpu::Context *)>(entry)(&c);
}
std::optional<NativeBlock> compile(const ir::Block &b, host::JitMemory &memory) {
  using namespace ir;
  static_assert(std::is_standard_layout_v<cpu::Context>);
  if (!host::JitMemory::supported() || b.instructions.empty())
    return std::nullopt;
  for (auto &i : b.instructions)
    if (!supported(i))
      return std::nullopt;
  Encoder e;
  NativeBlock result;
  e.emit(0xa9bf7bf3); // STP x19,x30,[sp,#-16]!
  e.emit(0xaa0003f3); // MOV x19,x0 (callee-saved context)
  for (auto &i : b.instructions) {
    result.pc_map.emplace_back(e.words.size() * 4, i.pc);
    e.immediate(9, i.pc);
    e.store(9, 19, offsetof(cpu::Context, rip), 64);
    if (i.op == Op::Nop)
      continue;
    cpu::FlagOp flag = cpu::FlagOp::None;
    bool store = true;
    if (i.op == Op::Mov)
      operand(e, 3, i.src);
    else {
      operand(e, 1, i.dst);
      if (i.src.kind != Kind::None)
        operand(e, 2, i.src);
      else
        e.immediate(2, 1);
      unsigned operation = 0;
      switch (i.op) {
      case Op::Add:
        flag = cpu::FlagOp::Add;
        break;
      case Op::Sub:
      case Op::Cmp:
        operation = 1;
        flag = cpu::FlagOp::Sub;
        store = i.op != Op::Cmp;
        break;
      case Op::And:
      case Op::Test:
        operation = 2;
        flag = cpu::FlagOp::Logic;
        store = i.op != Op::Test;
        break;
      case Op::Or:
        operation = 3;
        flag = cpu::FlagOp::Logic;
        break;
      case Op::Xor:
        operation = 4;
        flag = cpu::FlagOp::Logic;
        break;
      case Op::Inc:
        flag = cpu::FlagOp::Inc;
        break;
      case Op::Dec:
        operation = 1;
        flag = cpu::FlagOp::Dec;
        break;
      case Op::Neg:
        e.emit(0xaa0103e2);
        e.immediate(1, 0);
        operation = 1;
        flag = cpu::FlagOp::Sub;
        break;
      case Op::Not:
        e.immediate(2, cpu::mask(i.dst.width));
        operation = 4;
        break;
      default:
        throw std::logic_error("unsupported JIT instruction");
      }
      e.alu(operation, 3, 1, 2, i.dst.width == 64);
    }
    if (store)
      e.store(3, 19, unsigned(offsetof(cpu::Context, gpr)) + i.dst.reg * 8 + (i.dst.high8 ? 1 : 0),
              i.dst.width == 32 ? 64 : i.dst.width);
    if (flag != cpu::FlagOp::None) {
      e.emit(0xaa1303e0); // MOV x0,x19
      e.immediate(4, uint64_t(flag));
      e.immediate(5, i.dst.width);
      e.immediate(16, reinterpret_cast<uintptr_t>(&defer_flags));
      e.emit(0xd63f0200); // BLR x16
    }
  }
  auto &last = b.instructions.back();
  e.immediate(9, last.pc + last.length);
  e.store(9, 19, offsetof(cpu::Context, rip), 64);
  e.emit(0xa8c17bf3);
  e.emit(0xd65f03c0);
  if (!memory.fits(e.words.size() * 4))
    return std::nullopt;
  result.words = e.words;
  result.entry = memory.append(e.words);
  return result;
}
Dispatcher::Dispatcher(bool interp_only) {
  if (!interp_only && host::JitMemory::supported())
    code_ = std::make_unique<host::JitMemory>();
}
size_t Dispatcher::step(cpu::Context &c, mem::AddressSpace &m, std::ostream *trace, bool dump_ir,
                        bool dump_asm) {
  auto found = blocks_.find(c.rip);
  if (found != blocks_.end()) {
    bool valid = found->second.mode64 == c.mode64;
    for (auto [page, generation] : found->second.pages)
      if (m.generation(page) != generation)
        valid = false;
    if (!valid) {
      blocks_.erase(found);
      found = blocks_.end();
      ++stats.invalidations;
    }
  }
  if (found == blocks_.end()) {
    auto b = x86::block(m, c.rip, c.mode64 ? x86::Mode::X64 : x86::Mode::X86);
    // A store must end a block so a subsequent instruction cannot execute stale decoded bytes.
    for (size_t n = 0; n < b.instructions.size(); ++n) {
      const auto &i = b.instructions[n];
      if (i.dst.kind == ir::Kind::Memory || i.op == ir::Op::Push) {
        b.instructions.resize(n + 1);
        break;
      }
    }
    ir::optimize(b);
    Cached entry{b, std::nullopt, {}, c.mode64};
    if (code_ && c.mode64) {
      // Compile the largest supported prefix; memory/control operations get their own fallback
      // block.
      ir::Block prefix{b.pc, {}};
      for (auto &i : b.instructions) {
        if (!supported(i))
          break;
        prefix.instructions.push_back(i);
      }
      if (!prefix.instructions.empty()) {
        entry.ir = prefix;
        entry.native = compile(prefix, *code_);
        if (entry.native)
          ++stats.translated_blocks;
      }
    }
    auto end = entry.ir.instructions.back().pc + entry.ir.instructions.back().length - 1;
    for (auto page = c.rip / 4096 * 4096; page <= end / 4096 * 4096; page += 4096)
      entry.pages.emplace_back(page, m.generation(page));
    if (trace && dump_ir)
      for (auto &i : entry.ir.instructions)
        *trace << "IR " << ir::format(i) << '\n';
    if (trace && dump_asm && entry.native) {
      for (auto w : entry.native->words)
        *trace << "ARM64 0x" << std::hex << std::setw(8) << std::setfill('0') << w << std::dec
               << '\n';
    }
    found = blocks_.emplace(c.rip, std::move(entry)).first;
  } else
    ++stats.cache_hits;
  auto &block = found->second;
  if (trace)
    for (auto &i : block.ir.instructions)
      *trace << ir::format(i) << '\n';
  if (block.native)
    block.native->run(c);
  else {
    ++stats.interpreter_fallbacks;
    if (trace)
      *trace << "interpreter block at 0x" << std::hex << c.rip << std::dec << '\n';
    for (auto &i : block.ir.instructions)
      ir::execute(i, c, m);
  }
  auto count = block.ir.instructions.size();
  stats.instructions += count;
  return count;
}
std::optional<uint64_t> Dispatcher::guest_pc(const void *pc) const {
  auto address = reinterpret_cast<uintptr_t>(pc);
  for (auto &[key, block] : blocks_) {
    (void)key;
    if (!block.native)
      continue;
    auto base = reinterpret_cast<uintptr_t>(block.native->entry);
    if (address < base || address >= base + block.native->words.size() * 4)
      continue;
    uint64_t guest = block.ir.pc;
    for (auto [offset, gpc] : block.native->pc_map) {
      if (base + offset > address)
        break;
      guest = gpc;
    }
    return guest;
  }
  return std::nullopt;
}
} // namespace execon::jit
