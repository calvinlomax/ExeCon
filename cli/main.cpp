#include "engine/api/execon_engine.h"
#include "engine/cpu/fault.h"
#include "engine/host/process.h"
#include "engine/x86/decoder.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
namespace {
std::string env(const char *key, const std::string &fallback = "") {
  auto value = std::getenv(key);
  return value ? value : fallback;
}
execon::win32::MissingPolicy policy(const std::string &s) {
  using P = execon::win32::MissingPolicy;
  if (s == "abort")
    return P::Abort;
  if (s == "return-error")
    return P::ReturnError;
  if (s == "ignore-void")
    return P::IgnoreVoid;
  throw std::invalid_argument("invalid missing-API policy: " + s);
}
void trace(execon::Options &o, const std::string &s) {
  if (s.empty() || s == "off")
    return;
  if (s == "all" || s == "cpu") {
    o.trace = true;
    o.trace_api = s == "all";
  } else if (s == "api")
    o.trace_api = true;
  else
    throw std::invalid_argument(
        "trace must be cpu, api, all, or off; detailed memory tracing is not implemented");
}
void assignment(std::map<std::string, std::string> &values, const std::string &s) {
  auto p = s.find('=');
  if (p == std::string::npos || p == 0)
    throw std::invalid_argument("expected KEY=VALUE");
  values[s.substr(0, p)] = s.substr(p + 1);
}
void usage() {
  std::cout << R"(ExeCon 0.1.0 — experimental Windows console runner
Usage: execon [options] <program.exe> [program args...]
  --inspect                 Inspect PE metadata without execution
  --report-imports          Report implemented and missing imports
  --interp-only             Disable native ARM64 code generation
  --trace[=cpu|api|all]      Trace guest instructions/API dispatch
  --dump-ir --dump-asm --stats
  --prefix <directory>      Prefix directory (drive_c beneath it)
  --drive X=<directory>     Additional Windows drive mapping
  --cwd <Windows directory> Guest current directory
  --env KEY=VALUE           Guest environment entry (repeatable)
  --on-missing=abort|return-error|ignore-void
  --log-level <0..3> --log-file <path>
  --max-instructions <n>    Execution budget (default 10000000)
  --status-file <path>      JSON with full guest exit or engine failure
  --decoder-coverage --self-check --version --help

Engine statuses: 125 usage, 126 engine/missing feature, 127 guest fault.
Guest exit status is passed through modulo 256; --status-file distinguishes collisions.
Major options have EXECON_* environment equivalents. See README.md.
)";
}
} // namespace
int main(int argc, char **argv) {
  std::ofstream log_file;
  std::ostream *log = &std::cerr;
  std::string status_file;
  auto status = [&](const char *kind, uint32_t code) {
    if (!status_file.empty()) {
      std::ofstream out(status_file);
      if (!out)
        throw std::runtime_error("cannot write status file");
      out << "{\"kind\":\"" << kind << "\",\"code\":" << code << "}\n";
    }
  };
  try {
    execon::Options o;
    o.interp_only = env("EXECON_INTERP_ONLY") == "1";
    o.prefix = env("EXECON_PREFIX", env("HOME") + "/Library/Application Support/ExeCon/prefix");
    o.cwd = env("EXECON_CWD", "C:\\");
    o.missing = policy(env("EXECON_ON_MISSING", "abort"));
    trace(o, env("EXECON_TRACE"));
    o.stats = env("EXECON_STATS") == "1";
    o.dump_ir = env("EXECON_DUMP_IR") == "1";
    o.dump_asm = env("EXECON_DUMP_ASM") == "1";
    o.instruction_limit = std::stoull(env("EXECON_MAX_INSTRUCTIONS", "10000000"));
    int level = std::stoi(env("EXECON_LOG_LEVEL", "1"));
    std::string log_path = env("EXECON_LOG_FILE");
    status_file = env("EXECON_STATUS_FILE");
    std::istringstream entries(env("EXECON_ENV"));
    for (std::string line; std::getline(entries, line);)
      if (!line.empty())
        assignment(o.environment, line);
    bool inspect = false, report = false;
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
      std::string a = argv[i];
      auto value = [&]() {
        if (i + 1 >= argc)
          throw std::invalid_argument("missing value for " + a);
        return std::string(argv[++i]);
      };
      if (a == "--help") {
        usage();
        return 0;
      }
      if (a == "--version") {
        std::cout << "ExeCon " << execon_version() << '\n';
        return 0;
      }
      if (a == "--decoder-coverage") {
        std::cout << execon::x86::coverage();
        return 0;
      }
      if (a == "--self-check") {
        if (!execon::host::JitMemory::supported())
          throw std::runtime_error("JIT requires unsanitized Darwin arm64 build");
        execon::host::JitMemory arena;
        execon::ir::Instruction nop;
        nop.length = 1;
        auto block = execon::jit::compile({0, {nop}}, arena);
        execon::cpu::Context cpu;
        block->run(cpu);
        std::cout << "ExeCon JIT self-check passed; API " << EXECON_API_VERSION << '\n';
        return 0;
      }
      if (a == "--inspect")
        inspect = true;
      else if (a == "--report-imports")
        report = true;
      else if (a == "--interp-only")
        o.interp_only = true;
      else if (a == "--stats")
        o.stats = true;
      else if (a == "--dump-ir")
        o.dump_ir = true;
      else if (a == "--dump-asm")
        o.dump_asm = true;
      else if (a == "--trace")
        trace(o, "all");
      else if (a.starts_with("--trace="))
        trace(o, a.substr(8));
      else if (a.starts_with("--on-missing="))
        o.missing = policy(a.substr(13));
      else if (a == "--prefix")
        o.prefix = value();
      else if (a == "--cwd")
        o.cwd = value();
      else if (a == "--env")
        assignment(o.environment, value());
      else if (a == "--drive") {
        auto v = value();
        if (v.size() < 3 || v[1] != '=' || !std::isalpha(static_cast<unsigned char>(v[0])))
          throw std::invalid_argument("drive requires X=<directory>");
        o.drives[char(std::toupper(static_cast<unsigned char>(v[0])))] = v.substr(2);
      } else if (a == "--log-level")
        level = std::stoi(value());
      else if (a == "--log-file")
        log_path = value();
      else if (a == "--status-file")
        status_file = value();
      else if (a == "--max-instructions")
        o.instruction_limit = std::stoull(value());
      else if (a == "--") {
        for (++i; i < argc; ++i)
          args.emplace_back(argv[i]);
        break;
      } else if (a.starts_with('-'))
        throw std::invalid_argument("unknown option " + a);
      else {
        for (; i < argc; ++i)
          args.emplace_back(argv[i]);
        break;
      }
    }
    if (args.empty()) {
      usage();
      return 125;
    }
    if (level < 0 || level > 3)
      throw std::invalid_argument("log level must be 0..3");
    if (!o.instruction_limit)
      throw std::invalid_argument("instruction budget must be positive");
    if (level == 0) {
      o.trace = false;
      o.trace_api = false;
    }
    if (!log_path.empty()) {
      log_file.open(log_path, std::ios::app);
      if (!log_file)
        throw std::runtime_error("cannot open log file");
      log = &log_file;
    }
    auto p = execon::pe::Image::read_file(args[0]);
    if (report)
      execon::win32::Modules::report(p, std::cerr);
    if (inspect) {
      p.inspect(std::cout);
      return 0;
    }
    uint32_t code = execon::run(p, args, o, *log);
    status("guest", code);
    if (o.stats)
      *log << "guest_exit=" << code << '\n';
    return int(code & 255);
  } catch (const execon::Fault &e) {
    *log << "ExeCon guest fault 0x" << std::hex << e.code << " at guest PC 0x" << e.instruction_pc
         << " (address 0x" << e.address << ")" << std::dec << ": " << e.what() << '\n';
    try {
      status("guest-fault", e.code);
    } catch (...) {
    }
    return 127;
  } catch (const std::invalid_argument &e) {
    *log << "ExeCon usage: " << e.what() << '\n';
    try {
      status("usage", 125);
    } catch (...) {
    }
    return 125;
  } catch (const std::exception &e) {
    *log << "ExeCon: " << e.what() << '\n';
    try {
      status("engine", 126);
    } catch (...) {
    }
    return 126;
  }
}
