#include "engine/pe/image.h"
#include <iostream>
int main(int argc, char **argv) {
  try {
    if (argc == 3 && std::string(argv[1]) == "--inspect") {
      execon::pe::Image::read_file(argv[2]).inspect(std::cout);
      return 0;
    }
    std::cerr << "Usage: execon --inspect program.exe\nExecution backend is not yet available.\n";
    return 125;
  } catch (const std::exception &e) {
    std::cerr << "ExeCon: " << e.what() << '\n';
    return 126;
  }
}
