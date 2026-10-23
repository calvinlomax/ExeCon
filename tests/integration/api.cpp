#include "engine/api/execon_engine.h"
#include <iostream>
int main(int argc, char **argv) {
  if (argc != 2)
    return 1;
  for (int interpreter : {0, 1}) {
    uint32_t guest = 0;
    char error[512]{};
    const char *args[] = {"argument with spaces"};
    if (execon_run(argv[1], 1, args, interpreter, &guest, error, sizeof(error)) != 0 ||
        guest != 42 || error[0]) {
      std::cerr << "C API execution failed: " << error << " guest=" << guest << '\n';
      return 1;
    }
  }
  std::cout << "PASS C API full guest exit and sequential backend lifetimes\n";
}
