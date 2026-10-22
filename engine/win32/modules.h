#pragma once
#include "engine/cpu/context.h"
#include "engine/mem/address_space.h"
#include "engine/nt/services.h"
#include "engine/pe/image.h"
#include <map>
#include <optional>
#include <ostream>
namespace execon::win32 {
enum class MissingPolicy { Abort, ReturnError, IgnoreVoid };
struct ProcessState {
  uint64_t image_base = 0, teb = 0, command_a = 0, command_w = 0;
  bool exited = false;
  uint32_t exit_code = 0;
};
class Modules {
public:
  Modules(nt::Services &services, ProcessState &state, std::ostream &log, MissingPolicy policy,
          bool trace = false)
      : services_(services), state_(state), log_(log), policy_(policy), trace_(trace) {}
  void bind(const pe::Image &image, uint64_t base, mem::AddressSpace &memory);
  bool contains(uint64_t pc) const { return stubs_.contains(pc); }
  void call(cpu::Context &cpu, mem::AddressSpace &memory);
  static bool implemented(const std::string &dll, const std::string &name);
  static void report(const pe::Image &image, std::ostream &out);

private:
  nt::Services &services_;
  ProcessState &state_;
  std::ostream &log_;
  MissingPolicy policy_;
  bool trace_;
  std::map<uint64_t, pe::Import> stubs_;
  uint64_t invoke(const pe::Import &api, cpu::Context &cpu, mem::AddressSpace &memory);
};
std::string read_string(mem::AddressSpace &memory, uint64_t address, bool wide = false);
} // namespace execon::win32
