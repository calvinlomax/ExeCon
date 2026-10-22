#include "engine/host/log.h"
namespace execon {
void Log::write(int level, std::string_view message) {
  if (level <= level_)
    out_ << "ExeCon: " << message << '\n';
}
} // namespace execon
