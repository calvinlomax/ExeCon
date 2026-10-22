#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace execon::host {
class JitMemory {
public:
  explicit JitMemory(size_t capacity = 4 * 1024 * 1024);
  ~JitMemory();
  JitMemory(const JitMemory &) = delete;
  JitMemory &operator=(const JitMemory &) = delete;
  void *append(std::span<const uint32_t> code);
  static bool supported();
  size_t used() const { return used_; }
  bool fits(size_t bytes) const { return bytes <= capacity_ - used_; }

private:
  [[maybe_unused]] void *base_ = nullptr;
  [[maybe_unused]] size_t capacity_ = 0, used_ = 0;
};
} // namespace execon::host
