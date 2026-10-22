#pragma once
#include <string>
#include <string_view>
#include <vector>
namespace execon::nt {
std::u16string utf16(std::string_view utf8);
std::string utf8(std::u16string_view utf16);
std::string command_line(const std::vector<std::string> &arguments);
} // namespace execon::nt
