#include "engine/jit/backend.h"
#include "engine/x86/decoder.h"
#include <chrono>
#include <iostream>
int main() {
  try {
    std::vector<uint8_t> code = {0x48, 0x01, 0xd8, 0x48, 0x31, 0xc1,
                                 0x48, 0xff, 0xc2, 0x48, 0x39, 0xc8};
    execon::ir::Block block{0x1000, {}};
    for (size_t n = 0; n < code.size();) {
      auto i = execon::x86::decode(std::span(code).subspan(n), 0x1000 + n, execon::x86::Mode::X64);
      n += i.length;
      block.instructions.push_back(i);
    }
    execon::mem::AddressSpace memory;
    execon::cpu::Context interpreted, native;
    interpreted.gpr[3] = 17;
    native = interpreted;
    constexpr unsigned iterations = 200000;
    auto measure = [&](auto run) {
      auto start = std::chrono::steady_clock::now();
      for (unsigned n = 0; n < iterations; ++n)
        run();
      return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
          .count();
    };
    double ir_ms = measure([&] {
      for (auto &i : block.instructions)
        execon::ir::execute(i, interpreted, memory);
    });
    std::cout << "iterations=" << iterations
              << " guest_instructions=" << iterations * block.instructions.size()
              << " interpreter_ms=" << ir_ms << '\n';
    if (execon::host::JitMemory::supported()) {
      execon::host::JitMemory arena;
      auto compiled = execon::jit::compile(block, arena);
      if (!compiled)
        throw std::runtime_error("benchmark block did not translate");
      double jit_ms = measure([&] { compiled->run(native); });
      if (native.gpr != interpreted.gpr || native.flags() != interpreted.flags() ||
          native.rip != interpreted.rip)
        throw std::runtime_error("benchmark state mismatch");
      std::cout << "jit_ms=" << jit_ms << " native_bytes=" << compiled->words.size() * 4
                << " arithmetic_ratio=" << ir_ms / jit_ms << " checksum=" << native.gpr[0] << '\n';
    } else
      std::cout << "SKIP native benchmark: JIT unavailable\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
