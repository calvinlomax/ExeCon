#include "engine/api/execon_engine.h"
#include "engine/host/process.h"
#include <cstdio>
#include <iostream>
extern "C" int execon_run(const char *path, int argc, const char *const *argv, int interp,
                          uint32_t *exit, char *error, uint32_t capacity) {
  try {
    if (!path || argc < 0 || (argc && !argv) || !exit)
      throw std::runtime_error("invalid C API arguments");
    std::vector<std::string> args{path};
    for (int i = 0; i < argc; ++i) {
      if (!argv[i])
        throw std::runtime_error("null argument");
      args.emplace_back(argv[i]);
    }
    execon::Options options;
    options.interp_only = interp;
    *exit = execon::run(execon::pe::Image::read_file(path), args, options, std::cerr);
    if (error && capacity)
      error[0] = 0;
    return 0;
  } catch (const std::exception &e) {
    if (error && capacity)
      std::snprintf(error, capacity, "%s", e.what());
    return -1;
  } catch (...) {
    if (error && capacity)
      std::snprintf(error, capacity, "unknown engine error");
    return -1;
  }
}
extern "C" const char *execon_version() { return "0.1.0"; }
