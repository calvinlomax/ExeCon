#pragma once
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>
namespace test {
struct Case {
  std::string name;
  std::function<void()> run;
};
inline std::vector<Case> &cases() {
  static std::vector<Case> registry;
  return registry;
}
struct Register {
  Register(const char *name, std::function<void()> run) { cases().push_back({name, run}); }
};
inline void check(bool ok, const char *expr, const char *file, int line) {
  if (!ok)
    throw std::runtime_error(std::string(file) + ":" + std::to_string(line) + ": " + expr);
}
} // namespace test
#define TEST(name)                                                                                 \
  static void name();                                                                              \
  static test::Register reg_##name(#name, name);                                                   \
  static void name()
#define CHECK(expr) test::check(bool(expr), #expr, __FILE__, __LINE__)
#define THROWS(expr)                                                                               \
  do {                                                                                             \
    bool caught = false;                                                                           \
    try {                                                                                          \
      (void)(expr);                                                                                \
    } catch (const std::exception &) {                                                             \
      caught = true;                                                                               \
    }                                                                                              \
    CHECK(caught);                                                                                 \
  } while (false)
