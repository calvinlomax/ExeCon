#pragma once
#include <cstdint>
#include <vector>
namespace test {
inline void put(std::vector<uint8_t> &b, size_t o, uint64_t v, unsigned n) {
  for (unsigned i = 0; i < n; ++i)
    b.at(o + i) = uint8_t(v >> (i * 8));
}
inline std::vector<uint8_t> pe(bool x64 = true) {
  std::vector<uint8_t> b(1024);
  put(b, 0, 0x5a4d, 2);
  put(b, 0x3c, 0x80, 4);
  put(b, 0x80, 0x4550, 4);
  put(b, 0x84, x64 ? 0x8664 : 0x14c, 2);
  put(b, 0x86, 1, 2);
  put(b, 0x94, x64 ? 240 : 224, 2);
  put(b, 0x96, 0x22, 2);
  size_t o = 0x98;
  put(b, o, x64 ? 0x20b : 0x10b, 2);
  put(b, o + 16, 0x1000, 4);
  put(b, o + (x64 ? 24 : 28), x64 ? 0x140000000ULL : 0x400000, x64 ? 8 : 4);
  put(b, o + 32, 4096, 4);
  put(b, o + 36, 512, 4);
  put(b, o + 56, 8192, 4);
  put(b, o + 60, 512, 4);
  put(b, o + 68, 3, 2);
  put(b, o + (x64 ? 108 : 92), 16, 4);
  size_t s = o + (x64 ? 240 : 224);
  b[s] = '.';
  b[s + 1] = 't';
  put(b, s + 8, 512, 4);
  put(b, s + 12, 4096, 4);
  put(b, s + 16, 512, 4);
  put(b, s + 20, 512, 4);
  put(b, s + 36, 0x60000020, 4);
  b[512] = 0xc3;
  return b;
}
} // namespace test
