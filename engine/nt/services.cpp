#include "engine/nt/services.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
namespace execon::nt {
Services::File::~File() {
  if (fd >= 0)
    ::close(fd);
}
Services::Services(Paths paths) : paths_(std::move(paths)) {
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

uint64_t Services::open(const std::string &path, uint32_t access, uint32_t share,
                        uint32_t creation) {
  if (access & ~0xc0000000U || share & ~7U)
    throw Error(120, "unsupported file access/share flags");
  bool rd = access & 0x80000000U, wr = access & 0x40000000U;
  if (!rd && !wr)
    throw Error(120, "metadata-only open not implemented");
  auto resolved = paths_.resolve(path).string();
  for (auto &[handle, f] : handles_) {
    (void)handle;
    std::error_code ec;
    bool same = f->path == resolved ||
                (!f->path.empty() && std::filesystem::equivalent(f->path, resolved, ec));
    if (same && ((rd && !(f->share & 1)) || (wr && !(f->share & 2)) ||
                 (f->readable && !(share & 1)) || (f->writable && !(share & 2))))
      throw Error(32, "file sharing violation");
  }
  int flags = rd && wr ? O_RDWR : wr ? O_WRONLY : O_RDONLY;
  switch (creation) {
  case 1:
    flags |= O_CREAT | O_EXCL;
    break;
  case 2:
    flags |= O_CREAT | O_TRUNC;
    break;
  case 3:
    break;
  case 4:
    flags |= O_CREAT;
    break;
  case 5:
    flags |= O_TRUNC;
    break;
  default:
    throw Error(87, "invalid creation disposition");
  }
  if ((creation == 2 || creation == 5) && !wr)
    throw Error(5, "truncate requires write access");
  auto f = std::make_shared<File>();
  f->fd = ::open(resolved.c_str(), flags | O_CLOEXEC, 0600);
  f->readable = rd;
  f->writable = wr;
  f->path = resolved;
  f->share = share;
  if (f->fd < 0)
    throw Error(errno == ENOENT ? 2 : errno == EEXIST ? 80 : 5, std::strerror(errno));
  uint64_t handle = next_handle_;
  next_handle_ += 4;
  handles_.emplace(handle, std::move(f));
  return handle;
}
uint64_t Services::seek(uint64_t h, int64_t offset, uint32_t origin) {
  if (origin > 2)
    throw Error(87, "invalid seek origin");
  auto result = ::lseek(file(h).fd, offset,
                        origin == 0   ? SEEK_SET
                        : origin == 1 ? SEEK_CUR
                                      : SEEK_END);
  if (result < 0)
    throw Error(25, std::strerror(errno));
  return uint64_t(result);
}
uint64_t Services::file_size(uint64_t h) const {
  struct stat s {};
  if (::fstat(file(h).fd, &s) < 0)
    throw Error(6, std::strerror(errno));
  return uint64_t(s.st_size);
}
} // namespace execon::nt
