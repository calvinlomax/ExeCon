#include <cstdio>
#include <stdexcept>
int main() {
  try {
    throw std::runtime_error("test");
  } catch (const std::exception &) {
    std::puts("C++ caught");
    return 0;
  }
  return 1;
}
