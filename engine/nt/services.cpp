#include "engine/nt/services.h"
#include <cerrno>
#include <cstring>
#include <unistd.h>
namespace execon::nt {
Services::File::~File() {
  if (fd >= 0)
    ::close(fd);
}
Services::Services() {
  for (int n = 0; n < 3; ++n) {
    auto f = std::make_shared<File>();
    f->fd = ::dup(n);
    f->readable = n == 0;
    f->writable = n != 0;
    if (f->fd < 0)
      throw Error(6, "cannot duplicate host standard descriptor");
    handles_.emplace(0x100 + n * 4, std::move(f));
  }
}
const Services::File &Services::file(uint64_t handle) const {
  auto it = handles_.find(handle);
  if (it == handles_.end())
    throw Error(6, "invalid handle");
  return *it->second;
}
uint64_t Services::standard_handle(int32_t which) const {
  switch (which) {
  case -10:
    return 0x100;
  case -11:
    return 0x104;
  case -12:
    return 0x108;
  default:
    throw Error(87, "invalid standard handle selector");
  }
}
size_t Services::write(uint64_t h, std::span<const uint8_t> b) {
  auto &f = file(h);
  if (!f.writable)
    throw Error(5, "handle is not writable");
  size_t done = 0;
  while (done < b.size()) {
    ssize_t n = ::write(f.fd, b.data() + done, b.size() - done);
    if (n < 0) {
      if (errno == EINTR)
        continue;
      throw Error(errno == EPIPE ? 109 : 5, std::strerror(errno));
    }
    if (!n)
      throw Error(29, "zero-length host write");
    done += size_t(n);
  }
  return done;
}
std::vector<uint8_t> Services::read(uint64_t h, size_t count) {
  auto &f = file(h);
  if (!f.readable)
    throw Error(5, "handle is not readable");
  std::vector<uint8_t> b(count);
  ssize_t n;
  do {
    n = ::read(f.fd, b.data(), count);
  } while (n < 0 && errno == EINTR);
  if (n < 0)
    throw Error(30, std::strerror(errno));
  b.resize(size_t(n));
  return b;
}
void Services::close(uint64_t h) {
  if (!handles_.erase(h))
    throw Error(6, "invalid handle");
}
uint32_t Services::process_id() const { return uint32_t(::getpid()); }
} // namespace execon::nt
