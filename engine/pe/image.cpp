#include "engine/pe/image.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <limits>
#include <stdexcept>
namespace execon::pe {
namespace {
[[noreturn]] void bad(const std::string &m) { throw std::runtime_error("PE: " + m); }
uint64_t read(std::span<const uint8_t> b, size_t o, unsigned n) {
  if (o > b.size() || n > b.size() - o)
    bad("truncated structure at file offset " + std::to_string(o));
  uint64_t v = 0;
  for (unsigned i = 0; i < n; ++i)
    v |= uint64_t(b[o + i]) << (8 * i);
  return v;
}
uint32_t add(uint32_t a, uint64_t b) {
  if (b > UINT32_MAX - a)
    bad("RVA overflow");
  return a + uint32_t(b);
}
bool power2(uint32_t n) { return n && !(n & (n - 1)); }
uint64_t round_page(uint64_t n) { return (n + 4095) & ~uint64_t(4095); }
} // namespace
std::span<const uint8_t> Image::raw(uint32_t rva, size_t n) const {
  if (rva > image_size || n > image_size - rva)
    bad("RVA outside image");
  if (rva < headers_size && n <= headers_size - rva)
    return std::span(data_).subspan(rva, n);
  for (auto &s : sections) {
    if (rva >= s.rva && uint64_t(rva) - s.rva <= s.raw_size && n <= s.raw_size - (rva - s.rva))
      return std::span(data_).subspan(uint64_t(s.raw_offset) + rva - s.rva, n);
  }
  bad("metadata RVA has no file backing: " + std::to_string(rva));
}
uint64_t Image::value(uint32_t rva, unsigned n) const { return read(raw(rva, n), 0, n); }
std::string Image::string(uint32_t rva) const {
  std::string s;
  for (unsigned i = 0; i < 32768; ++i) {
    char c = char(value(add(rva, i), 1));
    if (!c)
      return s;
    s += c;
  }
  bad("unterminated or oversized string");
}
Image Image::parse(std::span<const uint8_t> b) {
  if (b.size() > 256 * 1024 * 1024)
    bad("file exceeds 256 MiB limit");
  if (read(b, 0, 2) != 0x5a4d)
    bad("missing MZ signature");
  size_t nt = read(b, 0x3c, 4);
  if (read(b, nt, 4) != 0x4550)
    bad("missing PE signature");
  auto machine = read(b, nt + 4, 2), count = read(b, nt + 6, 2), opt_size = read(b, nt + 20, 2);
  if (!count || count > 96)
    bad("invalid section count");
  size_t opt = nt + 24;
  auto magic = read(b, opt, 2);
  Image p;
  p.is64 = magic == 0x20b;
  if ((!p.is64 && magic != 0x10b) || machine != (p.is64 ? 0x8664 : 0x14c))
    bad("only x86 PE32 and x86-64 PE32+ supported");
  p.characteristics = read(b, nt + 22, 2);
  size_t dir_off = p.is64 ? 112 : 96;
  if (opt_size < dir_off || opt > b.size() || opt_size > b.size() - opt)
    bad("truncated optional header");
  p.base = read(b, opt + (p.is64 ? 24 : 28), p.is64 ? 8 : 4);
  p.entry = read(b, opt + 16, 4);
  p.section_alignment = read(b, opt + 32, 4);
  auto file_alignment = read(b, opt + 36, 4);
  p.image_size = read(b, opt + 56, 4);
  p.headers_size = read(b, opt + 60, 4);
  p.subsystem = read(b, opt + 68, 2);
  if (!power2(p.section_alignment) || !power2(file_alignment) ||
      p.section_alignment < file_alignment)
    bad("invalid alignment");
  if (!p.image_size || p.image_size > 256 * 1024 * 1024 || p.image_size % p.section_alignment ||
      !p.headers_size || p.headers_size > b.size() || p.headers_size > p.image_size ||
      p.entry >= p.image_size)
    bad("invalid image/header/entry bounds");
  if (p.base > UINT64_MAX - p.image_size || (!p.is64 && p.base + p.image_size > 0x100000000ULL))
    bad("image base overflow");
  auto ndir = read(b, opt + dir_off - 4, 4);
  if (ndir > 16 || dir_off + ndir * 8 > opt_size)
    bad("invalid directory count");
  for (size_t i = 0; i < ndir; ++i) {
    auto &d = p.directories[i];
    d.rva = read(b, opt + dir_off + i * 8, 4);
    d.size = read(b, opt + dir_off + i * 8 + 4, 4);
    uint64_t limit = i == 4 ? b.size() : p.image_size;
    if (d.size && (!d.rva || d.rva > limit || d.size > limit - d.rva))
      bad("directory outside image/file");
  }
  auto section_table = opt + opt_size;
  if (section_table + count * 40 > p.headers_size)
    bad("section table outside headers");
  for (size_t i = 0; i < count; ++i) {
    size_t o = section_table + i * 40;
    Section s{};
    for (size_t j = 0; j < 8; ++j) {
      char c = char(read(b, o + j, 1));
      if (!c)
        break;
      s.name += c;
    }
    s.virtual_size = read(b, o + 8, 4);
    s.rva = read(b, o + 12, 4);
    s.raw_size = read(b, o + 16, 4);
    s.raw_offset = read(b, o + 20, 4);
    s.characteristics = read(b, o + 36, 4);
    uint64_t size = std::max(s.virtual_size, s.raw_size);
    if (s.rva % p.section_alignment || s.rva < p.headers_size || s.rva > p.image_size ||
        size > p.image_size - s.rva)
      bad("invalid section virtual range");
    if (s.raw_size && (s.raw_offset % file_alignment || s.raw_offset < p.headers_size ||
                       s.raw_offset > b.size() || s.raw_size > b.size() - s.raw_offset))
      bad("invalid section file range");
    for (auto &old : p.sections) {
      if (size && uint64_t(s.rva) < uint64_t(old.rva) + std::max(old.virtual_size, old.raw_size) &&
          uint64_t(old.rva) < s.rva + size)
        bad("overlapping virtual sections");
      if (s.raw_size && old.raw_size &&
          uint64_t(s.raw_offset) < uint64_t(old.raw_offset) + old.raw_size &&
          uint64_t(old.raw_offset) < uint64_t(s.raw_offset) + s.raw_size)
        bad("overlapping raw sections");
    }
    p.sections.push_back(s);
  }
  p.data_.assign(b.begin(), b.end());
  // Eagerly parse bounded metadata; no untrusted pointers escape the parser.
  for (bool delay : {false, true}) {
    auto d = p.directories[delay ? 13 : 1];
    if (!d.size)
      continue;
    uint32_t stride = delay ? 32 : 20;
    bool ended = false;
    for (uint32_t offset = 0; offset <= d.size && stride <= d.size - offset; offset += stride) {
      uint32_t r = add(d.rva, offset);
      auto desc = p.raw(r, stride);
      if (std::all_of(desc.begin(), desc.end(), [](uint8_t v) { return !v; })) {
        ended = true;
        break;
      }
      if (delay && p.value(r, 4) != 1)
        bad("legacy VA-based delay imports unsupported");
      uint32_t name = p.value(add(r, delay ? 4 : 12), 4), iat = p.value(add(r, delay ? 12 : 16), 4),
               lookup = p.value(add(r, delay ? 16 : 0), 4);
      if (!lookup)
        lookup = iat;
      std::string dll = p.string(name);
      bool terminated = false;
      unsigned width = p.is64 ? 8 : 4;
      for (uint32_t j = 0; j < 65536; ++j) {
        uint64_t v = p.value(add(lookup, uint64_t(j) * width), width);
        if (!v) {
          terminated = true;
          break;
        }
        Import im{dll, "", 0, add(iat, uint64_t(j) * width), delay};
        p.raw(im.iat, width);
        uint64_t ordinal_bit = 1ULL << (width * 8 - 1);
        if (v & ordinal_bit) {
          if (v & ~(ordinal_bit | 0xffffULL))
            bad("invalid import ordinal");
          im.ordinal = v & 0xffff;
          im.name = "#" + std::to_string(im.ordinal);
        } else {
          if (v > UINT32_MAX - 2)
            bad("invalid import name RVA");
          im.name = p.string(uint32_t(v) + 2);
        }
        if (p.imports.size() >= 65536)
          bad("too many imports");
        p.imports.push_back(im);
      }
      if (!terminated)
        bad("unterminated import lookup table");
    }
    if (!ended)
      bad("unterminated import descriptors");
  }
  auto ed = p.directories[0];
  if (ed.size) {
    if (ed.size < 40)
      bad("short export directory");
    uint32_t first = p.value(add(ed.rva, 16), 4), nf = p.value(add(ed.rva, 20), 4),
             nn = p.value(add(ed.rva, 24), 4);
    uint32_t functions = p.value(add(ed.rva, 28), 4), names = p.value(add(ed.rva, 32), 4),
             ordinals = p.value(add(ed.rva, 36), 4);
    if (nf > 65536 || nn > 65536 || nn > nf || uint64_t(first) + nf > UINT32_MAX)
      bad("invalid export counts");
    for (uint32_t i = 0; i < nf; ++i) {
      uint32_t r = p.value(add(functions, uint64_t(i) * 4), 4);
      if (r >= p.image_size)
        bad("export outside image");
      Export ex{"", "", first + i, r};
      if (r >= ed.rva && uint64_t(r) < uint64_t(ed.rva) + ed.size)
        ex.forwarder = p.string(r);
      p.exports.push_back(ex);
    }
    for (uint32_t i = 0; i < nn; ++i) {
      uint32_t j = p.value(add(ordinals, uint64_t(i) * 2), 2);
      if (j >= nf)
        bad("invalid export ordinal");
      p.exports[j].name = p.string(p.value(add(names, uint64_t(i) * 4), 4));
    }
  }
  auto rd = p.directories[5];
  for (uint32_t o = 0; o < rd.size;) {
    if (rd.size - o < 8)
      bad("short relocation block");
    uint32_t page = p.value(add(rd.rva, o), 4), n = p.value(add(rd.rva, o + 4), 4);
    if (n < 8 || n % 2 || n > rd.size - o)
      bad("invalid relocation block size");
    for (uint32_t j = 8; j < n; j += 2) {
      uint16_t v = p.value(add(rd.rva, uint64_t(o) + j), 2), type = v >> 12;
      uint32_t r = add(page, v & 0xfff);
      if (type && (r >= p.image_size || (type == 10 ? 8U : 4U) > p.image_size - r))
        bad("relocation outside image");
      if (type)
        p.relocations.push_back({r, type});
    }
    o += n;
  }
  auto td = p.directories[9];
  if (td.size) {
    if (td.size < (p.is64 ? 40U : 24U))
      bad("short TLS directory");
    auto callbacks = p.value(add(td.rva, p.is64 ? 24 : 12), p.is64 ? 8 : 4);
    if (callbacks) {
      if (callbacks < p.base || callbacks - p.base > UINT32_MAX)
        bad("invalid TLS callback array");
      bool terminated = false;
      for (uint32_t i = 0; i < 1024; ++i) {
        auto v = p.value(add(uint32_t(callbacks - p.base), i * (p.is64 ? 8 : 4)), p.is64 ? 8 : 4);
        if (!v) {
          terminated = true;
          break;
        }
        if (v < p.base || v - p.base >= p.image_size)
          bad("TLS callback outside image");
        p.tls_callbacks.push_back(v);
      }
      if (!terminated)
        bad("unterminated TLS callbacks");
    }
  }
  auto pd = p.directories[3];
  if (pd.size && p.is64) {
    if (pd.size % 12)
      bad("invalid .pdata size");
    for (uint32_t o = 0; o < pd.size; o += 12) {
      RuntimeFunction f{uint32_t(p.value(add(pd.rva, o), 4)),
                        uint32_t(p.value(add(pd.rva, o + 4), 4)),
                        uint32_t(p.value(add(pd.rva, o + 8), 4))};
      if (f.begin >= f.end || f.end > p.image_size || f.unwind >= p.image_size ||
          (!p.runtime_functions.empty() && f.begin < p.runtime_functions.back().end))
        bad("invalid .pdata entry");
      p.runtime_functions.push_back(f);
    }
  }
  return p;
}
Image Image::read_file(const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in)
    bad("cannot open " + path.string());
  auto n = in.tellg();
  if (n < 0 || n > 256 * 1024 * 1024)
    bad("invalid file size");
  std::vector<uint8_t> b(static_cast<size_t>(n));
  in.seekg(0);
  if (!in.read(reinterpret_cast<char *>(b.data()), n))
    bad("read failed");
  return parse(b);
}
uint64_t Image::load(mem::AddressSpace &m, uint64_t requested_base) const {
  auto target = requested_base ? requested_base : base;
  if (section_alignment < 4096 || target % 4096)
    bad("loader requires page-aligned base and sections");
  if (target != base && relocations.empty())
    bad("rebasing requires relocations");
  if (!is64 && (target >= 0x100000000ULL || image_size > 0x100000000ULL - target))
    bad("PE32 mapping exceeds 4 GiB");
  for (auto &r : relocations)
    if (r.type != (is64 ? 10 : 3))
      bad("unsupported relocation type " + std::to_string(r.type));
  auto size = round_page(image_size);
  m.map(target, size, mem::Read | mem::Write);
  try {
    m.write(target, std::span(data_).first(headers_size));
    for (auto &s : sections)
      if (s.raw_size)
        m.write(target + s.rva, std::span(data_).subspan(s.raw_offset, s.raw_size));
    if (target != base)
      for (auto &r : relocations) {
        unsigned width = is64 ? 8 : 4;
        m.store(target + r.rva, m.load(target + r.rva, width) + (target - base), width);
      }
    m.protect(target, size, 0);
    m.protect(target, round_page(headers_size), mem::Read);
    for (auto &s : sections) {
      uint8_t flags = 0;
      if (s.characteristics & 0x40000000)
        flags |= mem::Read;
      if (s.characteristics & 0x80000000)
        flags |= mem::Write;
      if (s.characteristics & 0x20000000)
        flags |= mem::Execute;
      auto n = round_page(std::max(s.virtual_size, s.raw_size));
      if (n)
        m.protect(target + s.rva, n, flags);
    }
  } catch (...) {
    m.unmap(target, size);
    throw;
  }
  return target;
}
void Image::inspect(std::ostream &out) const {
  out << (is64 ? "PE32+ x86-64" : "PE32 x86") << " base=0x" << std::hex << base << " entry=0x"
      << entry << " image-size=0x" << image_size << std::dec << " subsystem=" << subsystem << '\n';
  for (auto &s : sections)
    out << "section " << s.name << " RVA=0x" << std::hex << s.rva << " size=0x" << s.virtual_size
        << " flags=0x" << s.characteristics << std::dec << '\n';
  for (auto &im : imports)
    out << (im.delay ? "delay-import " : "import ") << im.dll << '!' << im.name << " IAT=0x"
        << std::hex << im.iat << std::dec << '\n';
  for (auto &ex : exports)
    out << "export " << ex.ordinal << ' ' << ex.name << " RVA=0x" << std::hex << ex.rva << std::dec
        << (ex.forwarder.empty() ? "" : " -> " + ex.forwarder) << '\n';
  for (auto &r : relocations)
    out << "relocation type=" << r.type << " RVA=0x" << std::hex << r.rva << std::dec << '\n';
  for (auto v : tls_callbacks)
    out << "TLS callback VA=0x" << std::hex << v << std::dec << '\n';
  for (auto &f : runtime_functions)
    out << "runtime-function RVA=0x" << std::hex << f.begin << "..0x" << f.end << " unwind=0x"
        << f.unwind << std::dec << '\n';
  for (size_t i = 0; i < directories.size(); ++i)
    if (directories[i].size)
      out << "directory[" << i << "] RVA=0x" << std::hex << directories[i].rva << " size=0x"
          << directories[i].size << std::dec << '\n';
}
} // namespace execon::pe
