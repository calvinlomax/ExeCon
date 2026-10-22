#include "engine/nt/paths.h"
#include "engine/nt/services.h"
#include <algorithm>
#include <cctype>
#include <vector>
namespace execon::nt {
namespace {
std::string lower(std::string value) {
  for (auto &c : value)
    c = char(std::tolower(static_cast<unsigned char>(c)));
  return value;
}
void slashes(std::string &s) { std::replace(s.begin(), s.end(), '/', '\\'); }
} // namespace
Paths::Paths(std::filesystem::path prefix, std::string cwd,
             std::map<char, std::filesystem::path> drives)
    : cwd_(std::move(cwd)), drives_(std::move(drives)) {
  if (prefix.empty()) {
    const char *home = std::getenv("HOME");
    prefix = std::filesystem::path(home ? home : ".") / "Library/Application Support/ExeCon/prefix";
  }
  if (!drives_.contains('C'))
    drives_['C'] = prefix / "drive_c";
  if (!drives_.contains('Z'))
    drives_['Z'] = "/";
  for (auto &[letter, path] : drives_) {
    (void)letter;
    path = std::filesystem::absolute(path).lexically_normal();
  }
  slashes(cwd_);
  if (cwd_.size() < 3 || cwd_[1] != ':' || cwd_[2] != '\\')
    throw Error(123, "guest working directory must be drive-absolute");
  cwd_ = absolute(cwd_);
}
std::string Paths::absolute(std::string path) const {
  slashes(path);
  if (path.starts_with("\\\\?\\"))
    path.erase(0, 4);
  if (path.starts_with("\\\\") || path.starts_with("UNC\\"))
    throw Error(53, "UNC shares are not implemented");
  if (path.size() < 2 || path[1] != ':') {
    if (!path.empty() && path[0] == '\\')
      path = cwd_.substr(0, 2) + path;
    else
      path = cwd_ + "\\" + path;
  }
  if (path.size() < 3 || path[2] != '\\' || !std::isalpha(static_cast<unsigned char>(path[0])))
    throw Error(123, "drive-relative paths are not implemented");
  char drive = char(std::toupper(static_cast<unsigned char>(path[0])));
  if (!drives_.contains(drive))
    throw Error(15, "unmapped guest drive");
  std::vector<std::string> parts;
  size_t begin = 3;
  while (begin <= path.size()) {
    auto end = path.find('\\', begin);
    if (end == std::string::npos)
      end = path.size();
    auto part = path.substr(begin, end - begin);
    if (part == "..") {
      if (!parts.empty())
        parts.pop_back();
    } else if (!part.empty() && part != ".") {
      if (part.find_first_of(":*?\"<>|") != std::string::npos)
        throw Error(123, "invalid Windows path component");
      parts.push_back(part);
    }
    if (end == path.size())
      break;
    begin = end + 1;
  }
  std::string result(1, drive);
  result += ":\\";
  for (size_t i = 0; i < parts.size(); ++i) {
    if (i)
      result += '\\';
    result += parts[i];
  }
  return result;
}
std::filesystem::path Paths::resolve(std::string path) const {
  auto windows = absolute(std::move(path));
  auto result = drives_.at(windows[0]);
  size_t begin = 3;
  while (begin < windows.size()) {
    auto end = windows.find('\\', begin);
    if (end == std::string::npos)
      end = windows.size();
    auto part = windows.substr(begin, end - begin);
    auto device = lower(part.substr(0, part.find('.')));
    if (device == "nul")
      return "/dev/null";
    if (device == "con")
      return "/dev/tty";
    if (device == "prn" || device == "aux" || device.starts_with("com") ||
        device.starts_with("lpt")) {
      if (device == "prn" || device == "aux" ||
          (device.size() == 4 && device[3] >= '1' && device[3] <= '9'))
        throw Error(123, "Windows device is not implemented");
    }
    std::error_code ec;
    auto exact = result / part;
    if (!std::filesystem::exists(exact, ec) && std::filesystem::is_directory(result, ec)) {
      std::string matched;
      for (std::filesystem::directory_iterator it(result, ec), last; !ec && it != last;
           it.increment(ec)) {
        auto name = it->path().filename().string();
        if (lower(name) == lower(part)) {
          if (!matched.empty())
            throw Error(123, "ambiguous case-insensitive path");
          matched = name;
        }
      }
      if (!matched.empty())
        part = matched;
    }
    result /= part;
    if (end == windows.size())
      break;
    begin = end + 1;
  }
  return result;
}
} // namespace execon::nt
