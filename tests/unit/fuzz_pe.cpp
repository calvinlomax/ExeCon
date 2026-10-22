#include "engine/pe/image.h"
#include <cstddef>
#include <cstdint>
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  try {
    (void)execon::pe::Image::parse({data, size});
  } catch (const std::exception &) {
  }
  return 0;
}
