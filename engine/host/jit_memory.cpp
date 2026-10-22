#include "engine/host/jit_memory.h"
#include <cstring>
#include <stdexcept>
#if defined(__APPLE__) && defined(__aarch64__) && !defined(EXECON_SANITIZE)
#include <libkern/OSCacheControl.h>
#include <pthread.h>
#include <sys/mman.h>
#endif
namespace execon::host {
bool JitMemory::supported() {
#if defined(__APPLE__) && defined(__aarch64__) && !defined(EXECON_SANITIZE)
  return true;
#else
  return false;
#endif
}
JitMemory::JitMemory(size_t capacity) {
#if defined(__APPLE__) && defined(__aarch64__) && !defined(EXECON_SANITIZE)
  capacity_ = capacity;
  base_ = mmap(nullptr, capacity, PROT_READ | PROT_WRITE | PROT_EXEC,
               MAP_PRIVATE | MAP_ANON | MAP_JIT, -1, 0);
  if (base_ == MAP_FAILED) {
    base_ = nullptr;
    throw std::runtime_error(
        "MAP_JIT failed: sign execon with allow-jit entitlement or use --interp-only");
  }
#else
  (void)capacity;
#endif
}
JitMemory::~JitMemory() {
#if defined(__APPLE__) && defined(__aarch64__) && !defined(EXECON_SANITIZE)
  if (base_)
    munmap(base_, capacity_);
#endif
}
void *JitMemory::append(std::span<const uint32_t> code) {
#if defined(__APPLE__) && defined(__aarch64__) && !defined(EXECON_SANITIZE)
  size_t bytes = code.size_bytes();
  if (bytes > capacity_ - used_)
    throw std::runtime_error("JIT cache capacity exhausted (4 MiB)");
  auto *destination = static_cast<uint8_t *>(base_) + used_;
  pthread_jit_write_protect_np(0);
  std::memcpy(destination, code.data(), bytes);
  pthread_jit_write_protect_np(1);
  sys_icache_invalidate(destination, bytes);
  used_ += bytes;
  return destination;
#else
  (void)code;
  throw std::runtime_error("native JIT requires unsanitized Darwin arm64 build");
#endif
}
} // namespace execon::host
