#pragma once
#include "engine/mem/address_space.h"
#include <array>
#include <filesystem>
#include <ostream>
#include <span>
#include <string>
#include <vector>
namespace execon::pe {
struct Directory {
  uint32_t rva = 0, size = 0;
};
struct Section {
  std::string name;
  uint32_t rva, virtual_size, raw_offset, raw_size, characteristics;
};
struct Import {
  std::string dll, name;
  uint16_t ordinal = 0;
  uint32_t iat;
  bool delay = false;
};
struct Export {
  std::string name, forwarder;
  uint32_t ordinal, rva;
};
struct Relocation {
  uint32_t rva;
  uint16_t type;
};
struct RuntimeFunction {
  uint32_t begin, end, unwind;
};
class Image {
public:
  bool is64 = false;
  uint64_t base = 0;
  uint32_t entry = 0, image_size = 0, headers_size = 0, section_alignment = 0;
  uint16_t subsystem = 0, characteristics = 0;
  std::array<Directory, 16> directories{};
  std::vector<Section> sections;
  std::vector<Import> imports;
  std::vector<Export> exports;
  std::vector<Relocation> relocations;
  std::vector<uint64_t> tls_callbacks;
  std::vector<RuntimeFunction> runtime_functions;
  static Image parse(std::span<const uint8_t> bytes);
  static Image read_file(const std::filesystem::path &path);
  uint64_t load(mem::AddressSpace &memory, uint64_t requested_base = 0) const;
  void inspect(std::ostream &out) const;
  std::span<const uint8_t> raw(uint32_t rva, size_t n) const;

private:
  std::vector<uint8_t> data_;
  uint64_t value(uint32_t rva, unsigned n) const;
  std::string string(uint32_t rva) const;
};
} // namespace execon::pe
