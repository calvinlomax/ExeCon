#include "engine/nt/heap.h"
#include "engine/nt/services.h"
#include <algorithm>
namespace execon::nt {
uint64_t Heap::allocate(uint64_t size) {
  if (size > 64 * 1024 * 1024)
    throw Error(8, "heap request exceeds 64 MiB");
  auto mapped = (std::max<uint64_t>(size, 1) + 4095) & ~uint64_t(4095);
  if (next_ > 0x6f000000 || mapped > 0x6f000000 - next_)
    throw Error(8, "heap address arena exhausted");
  auto result = next_;
  try {
    memory_.map(result, mapped, mem::Read | mem::Write);
  } catch (const std::runtime_error &) {
    throw Error(8, "cannot allocate guest heap pages");
  }
  allocations_[result] = size;
  next_ += mapped;
  return result;
}
uint64_t Heap::size(uint64_t a) const {
  auto it = allocations_.find(a);
  if (it == allocations_.end())
    throw Error(87, "invalid heap pointer");
  return it->second;
}
void Heap::free(uint64_t a) {
  if (!a)
    return;
  auto n = size(a);
  memory_.unmap(a, (std::max<uint64_t>(n, 1) + 4095) & ~uint64_t(4095));
  allocations_.erase(a);
}
uint64_t Heap::reallocate(uint64_t a, uint64_t n) {
  auto old = size(a);
  auto result = allocate(n);
  memory_.write(result, memory_.read(a, std::min(old, n)));
  free(a);
  return result;
}
} // namespace execon::nt
