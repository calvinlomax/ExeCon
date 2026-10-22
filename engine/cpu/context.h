#pragma once
#include <array>
#include <cstdint>
namespace execon::cpu {
enum Register : unsigned {
  RAX,
  RCX,
  RDX,
  RBX,
  RSP,
  RBP,
  RSI,
  RDI,
  R8,
  R9,
  R10,
  R11,
  R12,
  R13,
  R14,
  R15
};
enum Flag : uint64_t { CF = 1, PF = 4, AF = 16, ZF = 64, SF = 128, DF = 1024, OF = 2048 };
constexpr uint64_t arithmetic_flags = CF | PF | AF | ZF | SF | OF;
enum class FlagOp : uint8_t { None, Add, Sub, Logic, Inc, Dec };
struct LazyFlags {
  FlagOp op = FlagOp::None;
  uint8_t width = 64;
  uint64_t lhs = 0, rhs = 0, result = 0;
};
struct Context {
  std::array<uint64_t, 16> gpr{};
  uint64_t rip = 0, rflags = 2, fs_base = 0, gs_base = 0;
  std::array<std::array<uint8_t, 16>, 16> xmm{};
  std::array<std::array<uint8_t, 10>, 8> x87{};
  uint32_t mxcsr = 0x1f80;
  uint16_t x87_control = 0x37f, x87_status = 0, x87_tag = 0xffff;
  bool mode64 = true;
  LazyFlags lazy{};
  uint64_t flags();
  void defer(FlagOp op, unsigned width, uint64_t a, uint64_t b, uint64_t result);
};
constexpr uint64_t mask(unsigned width) { return width == 64 ? UINT64_MAX : ((1ULL << width) - 1); }
bool condition(uint8_t cc, uint64_t flags);
} // namespace execon::cpu
