#pragma once
#include "engine/ir/ir.h"
#include <span>
#include <string>
namespace execon::x86 {
enum class Mode { X86, X64 };
ir::Instruction decode(std::span<const uint8_t> bytes, uint64_t pc, Mode mode);
ir::Instruction decode(const mem::AddressSpace &memory, uint64_t pc, Mode mode);
ir::Block block(const mem::AddressSpace &memory, uint64_t pc, Mode mode,
                unsigned max_instructions = 32);
std::string coverage();
} // namespace execon::x86
