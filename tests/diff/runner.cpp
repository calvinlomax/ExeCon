#include "engine/x86/decoder.h"
#include <iomanip>
#include <iostream>
#include <sstream>
std::vector<uint8_t> unhex(const std::string &s) {
  std::vector<uint8_t> b;
  for (size_t i = 0; i < s.size(); i += 2)
    b.push_back(uint8_t(std::stoul(s.substr(i, 2), nullptr, 16)));
  return b;
}
int main() {
  std::string line;
  while (std::getline(std::cin, line)) {
    try {
      std::istringstream in(line);
      std::string command, hex;
      unsigned bits;
      in >> command >> bits >> hex;
      auto bytes = unhex(hex);
      auto mode = bits == 64 ? execon::x86::Mode::X64 : execon::x86::Mode::X86;
      auto ins = execon::x86::decode(bytes, 0x100000, mode);
      if (command == "decode") {
        std::cout << unsigned(ins.length) << '\n';
        continue;
      }
      execon::cpu::Context c;
      c.mode64 = bits == 64;
      c.rip = 0x100000;
      for (auto &r : c.gpr)
        in >> std::hex >> r;
      in >> std::hex >> c.rflags;
      execon::mem::AddressSpace m(bits == 32);
      m.map(0x200000, 4096, execon::mem::Read | execon::mem::Write);
      std::vector<uint8_t> memory(4096);
      for (size_t i = 0; i < memory.size(); ++i)
        memory[i] = uint8_t(i * 13 + 7);
      m.write(0x200000, memory);
      execon::ir::execute(ins, c, m);
      for (auto r : c.gpr)
        std::cout << std::hex << r << ' ';
      std::cout << c.flags() << ' ' << c.rip << ' ' << unsigned(ins.length) << ' ';
      for (auto v : m.read(0x200000, 4096))
        std::cout << std::setw(2) << std::setfill('0') << unsigned(v);
      std::cout << '\n';
    } catch (const std::exception &e) {
      std::cout << "error " << e.what() << '\n';
    }
  }
}
