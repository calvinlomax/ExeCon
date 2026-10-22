#pragma once
#include <filesystem>
#include <map>
#include <string>
namespace execon::nt {
class Paths {
public:
  Paths(std::filesystem::path prefix, std::string cwd = "C:\\",
        std::map<char, std::filesystem::path> drives = {});
  std::filesystem::path resolve(std::string path) const;
  std::string absolute(std::string path) const;
  const std::string &cwd() const { return cwd_; }

private:
  std::string cwd_;
  std::map<char, std::filesystem::path> drives_;
};
} // namespace execon::nt
