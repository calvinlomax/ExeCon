#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <stdexcept>
#include <vector>
namespace execon::nt {
class Error : public std::runtime_error {
public:
  uint32_t code;
  Error(uint32_t code, const std::string &message) : std::runtime_error(message), code(code) {}
};
class Services {
public:
  Services();
  uint64_t standard_handle(int32_t which) const;
  size_t write(uint64_t handle, std::span<const uint8_t> bytes);
  std::vector<uint8_t> read(uint64_t handle, size_t count);
  void close(uint64_t handle);
  uint32_t process_id() const;

private:
  struct File {
    int fd;
    bool readable, writable;
    ~File();
  };
  std::map<uint64_t, std::shared_ptr<File>> handles_;
  const File &file(uint64_t handle) const;
};
} // namespace execon::nt
