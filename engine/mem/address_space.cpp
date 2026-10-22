#include "engine/mem/address_space.h"
#include "engine/cpu/fault.h"
#include <atomic>
#include <stdexcept>
namespace execon::mem {
void AddressSpace::range(uint64_t a, uint64_t n) const {
  if (a >= limit_ || n > limit_ - a)
    throw Fault(access_violation, a, "guest address outside address space");
}
void AddressSpace::aligned(uint64_t a, uint64_t n) const {
  range(a, n);
  if (!n || a % page_size || n % page_size)
    throw std::runtime_error("mapping must be page-aligned and nonempty");
}
void AddressSpace::map(uint64_t a, uint64_t n, uint8_t p) {
  aligned(a, n);
  if (n / page_size > 65536 || pages_.size() + n / page_size > 65536)
    throw std::runtime_error("guest physical memory limit (256 MiB)");
  for (uint64_t v = a; v < a + n; v += page_size)
    if (pages_.contains(v))
      throw std::runtime_error("overlapping guest mapping");
  for (uint64_t v = a; v < a + n; v += page_size)
    pages_.emplace(v, Page{{}, p, ++epoch_});
}
void AddressSpace::check(uint64_t a, uint64_t n, uint8_t p) const {
  range(a, n);
  if (!n)
    return;
  for (uint64_t v = a / page_size * page_size; v <= (a + n - 1) / page_size * page_size;
       v += page_size) {
    auto it = pages_.find(v);
    if (it == pages_.end() || (it->second.permissions & p) != p)
      throw Fault(access_violation, a, "unmapped or disallowed guest memory access");
  }
}
void AddressSpace::protect(uint64_t a, uint64_t n, uint8_t p) {
  aligned(a, n);
  check(a, n, 0);
  for (uint64_t v = a; v < a + n; v += page_size) {
    pages_.at(v).permissions = p;
    pages_.at(v).generation = ++epoch_;
  }
}
void AddressSpace::unmap(uint64_t a, uint64_t n) {
  aligned(a, n);
  check(a, n, 0);
  for (uint64_t v = a; v < a + n; v += page_size)
    pages_.erase(v);
  ++epoch_;
}
std::vector<uint8_t> AddressSpace::read(uint64_t a, size_t n, uint8_t p) const {
  check(a, n, p);
  std::atomic_thread_fence(std::memory_order_seq_cst);
  std::vector<uint8_t> result(n);
  for (size_t i = 0; i < n; ++i)
    result[i] = pages_.at((a + i) / page_size * page_size).data[(a + i) % page_size];
  return result;
}
void AddressSpace::write(uint64_t a, std::span<const uint8_t> bytes) {
  check(a, bytes.size(), Write);
  for (size_t i = 0; i < bytes.size(); ++i) {
    auto &p = pages_.at((a + i) / page_size * page_size);
    p.data[(a + i) % page_size] = bytes[i];
    p.generation = ++epoch_;
  }
  std::atomic_thread_fence(std::memory_order_seq_cst);
}
uint64_t AddressSpace::load(uint64_t a, unsigned n, uint8_t p) const {
  if (n != 1 && n != 2 && n != 4 && n != 8)
    throw std::logic_error("invalid scalar size");
  auto b = read(a, n, p);
  uint64_t v = 0;
  for (unsigned i = 0; i < n; ++i)
    v |= uint64_t(b[i]) << (i * 8);
  return v;
}
void AddressSpace::store(uint64_t a, uint64_t v, unsigned n) {
  if (n != 1 && n != 2 && n != 4 && n != 8)
    throw std::logic_error("invalid scalar size");
  std::array<uint8_t, 8> bytes{};
  for (unsigned i = 0; i < n; ++i)
    bytes[i] = uint8_t(v >> (8 * i));
  write(a, std::span(bytes).first(n));
}
uint64_t AddressSpace::generation(uint64_t a) const {
  auto it = pages_.find(a / page_size * page_size);
  return it == pages_.end() ? 0 : it->second.generation;
}
uint8_t AddressSpace::permissions(uint64_t a) const {
  check(a, 1, 0);
  return pages_.at(a / page_size * page_size).permissions;
}
} // namespace execon::mem
