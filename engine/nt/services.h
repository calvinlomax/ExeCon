#pragma once
#include "engine/nt/paths.h"
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
  explicit Services(Paths paths = Paths({}, "C:\\"));
  uint64_t open(const std::string &path, uint32_t access, uint32_t share, uint32_t creation);
  uint64_t seek(uint64_t handle, int64_t offset, uint32_t origin);
  uint64_t file_size(uint64_t handle) const;
  const Paths &paths() const { return paths_; }
  uint64_t standard_handle(int32_t which) const;
  size_t write(uint64_t handle, std::span<const uint8_t> bytes);
  std::vector<uint8_t> read(uint64_t handle, size_t count);
  void close(uint64_t handle);
  uint32_t process_id() const;

private:
  struct File {
    int fd;
    bool readable, writable;
    std::string path;
    uint32_t share = 3;
    ~File();
  };
  std::map<uint64_t, std::shared_ptr<File>> handles_;
  Paths paths_;
  uint64_t next_handle_ = 0x110;
  const File &file(uint64_t handle) const;
};
} // namespace execon::nt
