#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
namespace execon {
class Fault : public std::runtime_error {
public:
  uint32_t code;
  uint64_t address;
  uint64_t instruction_pc = 0;
  Fault(uint32_t code, uint64_t address, const std::string &message)
      : std::runtime_error(message), code(code), address(address) {}
};
constexpr uint32_t access_violation = 0xc0000005;
constexpr uint32_t illegal_instruction = 0xc000001d;
} // namespace execon
