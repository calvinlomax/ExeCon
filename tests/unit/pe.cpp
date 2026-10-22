#include "engine/pe/image.h"
#include "tests/unit/fixture.h"
#include "tests/unit/test.h"
#include <random>
using namespace execon;
TEST(pe_headers_and_permissions) {
  for (bool x64 : {false, true}) {
    auto b = test::pe(x64);
    auto p = pe::Image::parse(b);
    CHECK(p.is64 == x64);
    CHECK(p.sections.size() == 1);
    mem::AddressSpace m(!x64);
    auto base = p.load(m);
    CHECK(m.load(base + 0x1000, 1, mem::Execute) == 0xc3);
    THROWS(m.store(base + 0x1000, 0, 1));
    THROWS(m.load(base, 1, mem::Execute));
    CHECK(m.load(base + 0x1001, 1) == 0);
  }
}
TEST(pe_truncation_and_mutation) {
  auto b = test::pe();
  for (size_t n = 0; n < b.size(); ++n)
    THROWS(pe::Image::parse(std::span(b).first(n)));
  std::mt19937 rng(0x5045);
  for (unsigned i = 0; i < 10000; ++i) {
    auto v = b;
    for (unsigned j = 0; j < 4; ++j)
      v[rng() % v.size()] = uint8_t(rng());
    try {
      (void)pe::Image::parse(v);
    } catch (const std::runtime_error &) {
    }
  }
}
TEST(pe_relocation) {
  auto b = test::pe();
  size_t d = 0x98 + 112 + 5 * 8;
  test::put(b, d, 0x1100, 4);
  test::put(b, d + 4, 12, 4);
  test::put(b, 0x300, 0x1000, 4);
  test::put(b, 0x304, 12, 4);
  test::put(b, 0x308, 0xa080, 2);
  test::put(b, 0x280, 0x140001234ULL, 8);
  auto p = pe::Image::parse(b);
  mem::AddressSpace m;
  p.load(m, 0x180000000ULL);
  CHECK(m.load(0x180001080ULL, 8) == 0x180001234ULL);
  test::put(b, 0x304, 0, 4);
  THROWS(pe::Image::parse(b));
}
TEST(memory_atomic_faults_and_generations) {
  mem::AddressSpace m(true);
  m.map(0x1000, 4096, mem::Read | mem::Write);
  auto gen = m.generation(0x1000);
  m.store(0x1000, 0x1234, 2);
  CHECK(m.load(0x1000, 2) == 0x1234);
  CHECK(m.generation(0x1000) > gen);
  m.store(0x1fff, 0x56, 1);
  THROWS(m.store(0x1fff, 0xabcd, 2));
  CHECK(m.load(0x1fff, 1) == 0x56);
  THROWS(m.map(0xfffff000, 8192, mem::Read));
  THROWS(m.load(UINT64_MAX, 8));
  m.unmap(0x1000, 4096);
  THROWS(m.load(0x1000, 1));
}
TEST(pe_imports_exports_tls_and_pdata) {
  auto b = test::pe();
  size_t dirs = 0x98 + 112;
  test::put(b, dirs + 8, 0x1100, 4);
  test::put(b, dirs + 12, 40, 4);
  test::put(b, 0x300, 0x1140, 4);
  test::put(b, 0x30c, 0x1180, 4);
  test::put(b, 0x310, 0x1160, 4);
  test::put(b, 0x340, 0x1190, 8);
  test::put(b, 0x360, 0x1190, 8);
  const std::string dll = "kernel32.dll", symbol = "ExitProcess";
  std::copy(dll.begin(), dll.end(), b.begin() + 0x380);
  std::copy(symbol.begin(), symbol.end(), b.begin() + 0x392);
  auto p = pe::Image::parse(b);
  CHECK(p.imports.size() == 1);
  CHECK(p.imports[0].name == symbol);
  CHECK(p.imports[0].dll == dll);
  auto broken = b;
  test::put(broken, 0x340, 0xffffffff, 8);
  THROWS(pe::Image::parse(broken));
  b = test::pe();
  test::put(b, dirs, 0x1100, 4);
  test::put(b, dirs + 4, 0x90, 4);
  test::put(b, 0x310, 1, 4);
  test::put(b, 0x314, 1, 4);
  test::put(b, 0x318, 1, 4);
  test::put(b, 0x31c, 0x1140, 4);
  test::put(b, 0x320, 0x1148, 4);
  test::put(b, 0x324, 0x1150, 4);
  test::put(b, 0x340, 0x1160, 4);
  test::put(b, 0x348, 0x1180, 4);
  const std::string forward = "kernel32.ExitProcess";
  std::copy(forward.begin(), forward.end(), b.begin() + 0x360);
  b[0x380] = 'F';
  p = pe::Image::parse(b);
  CHECK(p.exports[0].forwarder == forward);
  CHECK(p.exports[0].name == "F");
  b = test::pe();
  test::put(b, dirs + 9 * 8, 0x1100, 4);
  test::put(b, dirs + 9 * 8 + 4, 40, 4);
  test::put(b, 0x318, 0x140001140ULL, 8);
  test::put(b, 0x340, 0x140001000ULL, 8);
  p = pe::Image::parse(b);
  CHECK(p.tls_callbacks.size() == 1);
  test::put(b, dirs + 3 * 8, 0x1180, 4);
  test::put(b, dirs + 3 * 8 + 4, 12, 4);
  test::put(b, 0x380, 0x1000, 4);
  test::put(b, 0x384, 0x1010, 4);
  test::put(b, 0x388, 0x1190, 4);
  p = pe::Image::parse(b);
  CHECK(p.runtime_functions.size() == 1);
  test::put(b, 0x384, 0xfff, 4);
  THROWS(pe::Image::parse(b));
}
