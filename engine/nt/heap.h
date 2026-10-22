#pragma once
#include "engine/mem/address_space.h"
#include <map>
namespace execon::nt {
// Initial process heap: checked page-backed allocations. No movable handles or custom heaps.
class Heap {
public:
  explicit Heap(mem::AddressSpace &memory) : memory_(memory) {}
  uint64_t allocate(uint64_t size);
  void free(uint64_t address);
  uint64_t reallocate(uint64_t address, uint64_t size);
  uint64_t size(uint64_t address) const;

private:
  mem::AddressSpace &memory_;
  std::map<uint64_t, uint64_t> allocations_;
  uint64_t next_ = 0x50000000;
};
} // namespace execon::nt
