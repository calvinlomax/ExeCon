#pragma once
#include "engine/cpu/context.h"
#include "engine/mem/address_space.h"
#include <array>
#include <string>
#include <vector>
namespace execon::ir {
enum class Op : uint8_t {
  Nop,
  Mov,
  Movzx,
  Movsx,
  Lea,
  Add,
  Sub,
  And,
  Or,
  Xor,
  Cmp,
  Test,
  Inc,
  Dec,
  Neg,
  Not,
  Push,
  Pop,
  Call,
  Jump,
  Branch,
  Ret,
  Setcc,
  Cmov,
  Shl,
  Shr,
  Sar,
  Cld,
  Std,
  Cpuid,
  Rdtsc,
  Breakpoint,
  HostCall
};
enum class Kind : uint8_t { None, Register, Immediate, Memory };
enum class Segment : uint8_t { Flat, FS, GS };
enum class Ordering : uint8_t { SequentiallyConsistent };
struct Operand {
  Kind kind = Kind::None;
  uint8_t width = 0, reg = 0;
  bool high8 = false;
  uint64_t immediate = 0;
  int8_t base = -1, index = -1;
  uint8_t scale = 1, address_width = 64;
  int64_t displacement = 0;
  bool rip_relative = false;
  Segment segment = Segment::Flat;
};
struct Instruction {
  Op op = Op::Nop;
  Operand dst{}, src{};
  uint64_t pc = 0;
  uint8_t length = 0, condition = 0;
  std::array<uint8_t, 15> bytes{};
  Ordering ordering = Ordering::SequentiallyConsistent;
  bool writes_flags() const;
  bool terminates() const;
};
struct Block {
  uint64_t pc = 0;
  std::vector<Instruction> instructions;
};
uint64_t address(const Operand &op, const cpu::Context &cpu, uint64_t next_pc, bool segment = true);
uint64_t read(const Operand &op, cpu::Context &cpu, mem::AddressSpace &memory, uint64_t next_pc);
void write(const Operand &op, cpu::Context &cpu, mem::AddressSpace &memory, uint64_t next_pc,
           uint64_t value);
void execute(const Instruction &instruction, cpu::Context &cpu, mem::AddressSpace &memory);
void optimize(Block &block);
std::string format(const Instruction &instruction);
std::string op_name(Op op);
} // namespace execon::ir
