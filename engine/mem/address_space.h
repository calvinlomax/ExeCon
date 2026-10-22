#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <span>
#include <vector>
namespace execon::mem {
enum Permission : uint8_t { Read = 1, Write = 2, Execute = 4 };
class AddressSpace {
public:
  static constexpr uint64_t page_size = 4096;
  explicit AddressSpace(bool mode32 = false) : limit_(mode32 ? 0x100000000ULL : (1ULL << 47)) {}
  void map(uint64_t address, uint64_t size, uint8_t permissions);
  void protect(uint64_t address, uint64_t size, uint8_t permissions);
  void unmap(uint64_t address, uint64_t size);
  void check(uint64_t address, uint64_t size, uint8_t permission) const;
  std::vector<uint8_t> read(uint64_t address, size_t size, uint8_t permission = Read) const;
  void write(uint64_t address, std::span<const uint8_t> bytes);
  uint64_t load(uint64_t address, unsigned size, uint8_t permission = Read) const;
  void store(uint64_t address, uint64_t value, unsigned size);
  uint64_t generation(uint64_t address) const;
  uint8_t permissions(uint64_t address) const;

private:
  struct Page {
    std::array<uint8_t, page_size> data{};
    uint8_t permissions;
    uint64_t generation;
  };
  std::map<uint64_t, Page> pages_;
  uint64_t limit_;
  uint64_t epoch_ = 0;
  void range(uint64_t address, uint64_t size) const;
  void aligned(uint64_t address, uint64_t size) const;
};
} // namespace execon::mem
