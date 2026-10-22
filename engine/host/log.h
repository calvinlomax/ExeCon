#pragma once
#include <ostream>
#include <string_view>
namespace execon {
class Log {
public:
  explicit Log(std::ostream &out, int level = 1) : out_(out), level_(level) {}
  void write(int level, std::string_view message);

private:
  std::ostream &out_;
  int level_;
};
} // namespace execon
