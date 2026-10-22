#pragma once
#include <cstdint>
#include <vector>
namespace execon::jit {
class Encoder {
public:
  std::vector<uint32_t> words;
  void emit(uint32_t word) { words.push_back(word); }
  void immediate(unsigned dst, uint64_t value);
  void load(unsigned dst, unsigned base, unsigned offset, unsigned width);
  void store(unsigned src, unsigned base, unsigned offset, unsigned width);
  void alu(unsigned operation, unsigned dst, unsigned lhs, unsigned rhs, bool wide);
};
} // namespace execon::jit
