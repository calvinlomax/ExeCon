#pragma once
#include "engine/host/jit_memory.h"
#include "engine/ir/ir.h"
#include <map>
#include <memory>
#include <optional>
#include <ostream>
namespace execon::jit {
struct NativeBlock {
  void *entry = nullptr;
  std::vector<uint32_t> words;
  std::vector<std::pair<size_t, uint64_t>> pc_map;
  void run(cpu::Context &context) const;
};
std::optional<NativeBlock> compile(const ir::Block &block, host::JitMemory &memory);
struct Stats {
  uint64_t cache_hits = 0, translated_blocks = 0, interpreter_fallbacks = 0, invalidations = 0,
           instructions = 0;
};
class Dispatcher {
public:
  explicit Dispatcher(bool interp_only = false);
  size_t step(cpu::Context &cpu, mem::AddressSpace &memory, std::ostream *trace = nullptr,
              bool dump_ir = false, bool dump_asm = false);
  Stats stats;
  std::optional<uint64_t> guest_pc(const void *host_pc) const;

private:
  struct Cached {
    ir::Block ir;
    std::optional<NativeBlock> native;
    std::vector<std::pair<uint64_t, uint64_t>> pages;
    bool mode64;
  };
  std::unique_ptr<host::JitMemory> code_;
  std::map<uint64_t, Cached> blocks_;
};
} // namespace execon::jit
