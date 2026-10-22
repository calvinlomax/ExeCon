#include "engine/nt/text.h"
#include <stdexcept>
namespace execon::nt {
std::u16string utf16(std::string_view s) {
  std::u16string out;
  for (size_t i = 0; i < s.size();) {
    auto first = uint8_t(s[i++]);
    uint32_t cp;
    unsigned n;
    if (first < 0x80) {
      cp = first;
      n = 0;
    } else if (first >= 0xc2 && first <= 0xdf) {
      cp = first & 31;
      n = 1;
    } else if (first >= 0xe0 && first <= 0xef) {
      cp = first & 15;
      n = 2;
    } else if (first >= 0xf0 && first <= 0xf4) {
      cp = first & 7;
      n = 3;
    } else
      throw std::runtime_error("invalid UTF-8 leading byte");
    if (n > s.size() - i)
      throw std::runtime_error("truncated UTF-8");
    for (unsigned j = 0; j < n; ++j) {
      auto b = uint8_t(s[i++]);
      if ((b & 0xc0) != 0x80)
        throw std::runtime_error("invalid UTF-8 continuation");
      cp = (cp << 6) | (b & 63);
    }
    if ((n == 1 && cp < 0x80) || (n == 2 && cp < 0x800) || (n == 3 && cp < 0x10000) ||
        cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
      throw std::runtime_error("invalid UTF-8 code point");
    if (cp < 0x10000)
      out += char16_t(cp);
    else {
      cp -= 0x10000;
      out += char16_t(0xd800 + (cp >> 10));
      out += char16_t(0xdc00 + (cp & 1023));
    }
  }
  return out;
}
std::string utf8(std::u16string_view s) {
  std::string out;
  for (size_t i = 0; i < s.size(); ++i) {
    uint32_t cp = s[i];
    if (cp >= 0xd800 && cp <= 0xdbff) {
      if (i + 1 >= s.size() || s[i + 1] < 0xdc00 || s[i + 1] > 0xdfff)
        throw std::runtime_error("unpaired UTF-16 surrogate");
      cp = 0x10000 + ((cp - 0xd800) << 10) + (s[++i] - 0xdc00);
    } else if (cp >= 0xdc00 && cp <= 0xdfff)
      throw std::runtime_error("unpaired UTF-16 surrogate");
    if (cp < 0x80)
      out += char(cp);
    else if (cp < 0x800) {
      out += char(0xc0 | (cp >> 6));
      out += char(0x80 | (cp & 63));
    } else if (cp < 0x10000) {
      out += char(0xe0 | (cp >> 12));
      out += char(0x80 | ((cp >> 6) & 63));
      out += char(0x80 | (cp & 63));
    } else {
      out += char(0xf0 | (cp >> 18));
      out += char(0x80 | ((cp >> 12) & 63));
      out += char(0x80 | ((cp >> 6) & 63));
      out += char(0x80 | (cp & 63));
    }
  }
  return out;
}
std::string command_line(const std::vector<std::string> &args) {
  std::string result;
  for (auto &arg : args) {
    if (!result.empty())
      result += ' ';
    bool quote = arg.empty() || arg.find_first_of(" \t\n\v\"") != std::string::npos;
    if (!quote) {
      result += arg;
      continue;
    }
    result += '"';
    size_t slashes = 0;
    for (char c : arg) {
      if (c == '\\') {
        ++slashes;
        continue;
      }
      if (c == '"')
        result.append(slashes * 2 + 1, '\\');
      else
        result.append(slashes, '\\');
      slashes = 0;
      result += c;
    }
    result.append(slashes * 2, '\\');
    result += '"';
  }
  return result;
}
} // namespace execon::nt
