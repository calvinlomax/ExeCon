#include "engine/host/log.h"
#include "tests/unit/test.h"
#include <iostream>
#include <sstream>
TEST(log_filters) {
  std::ostringstream out;
  execon::Log log(out);
  log.write(2, "hidden");
  log.write(1, "visible");
  CHECK(out.str() == "ExeCon: visible\n");
}
int main() {
  unsigned failed = 0, skipped = 0;
  for (const auto &t : test::cases()) {
    try {
      t.run();
      std::cout << "PASS " << t.name << '\n';
    } catch (const test::Skip &e) {
      ++skipped;
      std::cout << "SKIP " << t.name << ": " << e.what() << '\n';
    } catch (const std::exception &e) {
      ++failed;
      std::cerr << "FAIL " << t.name << ": " << e.what() << '\n';
    }
  }
  std::cout << test::cases().size() << " tests, " << failed << " failures, " << skipped
            << " skipped\n";
  return failed ? 1 : 0;
}
