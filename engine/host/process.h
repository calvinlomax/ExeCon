#pragma once
#include "engine/jit/backend.h"
#include "engine/win32/modules.h"
#include <filesystem>
#include <map>
namespace execon {
struct Options {
  bool interp_only = false, trace = false, trace_api = false, dump_ir = false, dump_asm = false,
       stats = false;
  win32::MissingPolicy missing = win32::MissingPolicy::Abort;
  std::filesystem::path prefix, cwd;
  std::map<char, std::filesystem::path> drives;
  std::map<std::string, std::string> environment;
  uint64_t instruction_limit = 10000000;
};
uint32_t run(const pe::Image &image, const std::vector<std::string> &args, const Options &options,
             std::ostream &log);
} // namespace execon
