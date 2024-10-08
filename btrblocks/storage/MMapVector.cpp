#include "MMapVector.hpp"
#include <numeric>
#include <vector>
using namespace std;

// -------------------------------------------------------------------------------------
void btrblocks::writeBinary(const char* pathname, std::vector<std::string>& v) {
  using Data = Vector<std::string_view>::Data;
  // std::cout << "Writing binary file : " << pathname << std::endl;
  int fd = open(pathname, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  die_if(fd != -1);
  uint64_t fileSize = 8 + 16 * v.size();
  for (const auto& s : v) {
    fileSize += s.size() + 1;
  }
#if defined(__linux__)
  die_if(posix_fallocate(fd, 0, fileSize) == 0);
#elif defined(__APPLE__)
  fstore_t store = {F_ALLOCATECONTIG, F_PEOFPOSMODE, 0, static_cast<off_t>(fileSize)};
  // Try to get a continous chunk of disk space
  int ret = fcntl(fd, F_PREALLOCATE, &store);
  if (-1 == ret) {
    // OK, perhaps we are too fragmented, allocate non-continuous
    store.fst_flags = F_ALLOCATEALL;
    ret = fcntl(fd, F_PREALLOCATE, &store);
    if (-1 == ret)
      die_if(false);
  }
  die_if(0 == ftruncate(fd, fileSize));
#endif
  auto data =
      reinterpret_cast<Data*>(mmap(nullptr, fileSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
  data->count = v.size();
  die_if(data != MAP_FAILED);
  uint64_t offset = 8 + 16 * v.size();
  char* dst = reinterpret_cast<char*>(data);
  uint64_t slot = 0;
  for (auto s : v) {
    data->slot[slot].size = s.size();
    data->slot[slot].offset = offset;
    strcpy(dst + offset, s.data());
    offset += s.size();
    slot++;
  }
  die_if(close(fd) == 0);
}

namespace btrblocks {
using Data = btrblocks::Vector<std::string_view>::Data;

Data* writeInMem(const std::vector<std::string_view>& v, uint64_t& memSize) {
  const uint64_t initial_offset = sizeof(size_t) + sizeof(StringIndexSlot) * v.size();
  memSize = std::accumulate(v.begin(), v.end(), initial_offset,
    [](uint64_t acc, const std::string_view& s) { return acc + s.size(); });

  const auto data = reinterpret_cast<Data*>(new char[memSize]);
  die_if(data != nullptr);
  data->count = v.size();
  
  uint64_t cur_offset = initial_offset;
  char* dst = reinterpret_cast<char*>(data);
  uint64_t slot = 0;
  for (const auto s : v) {
    data->slot[slot].size = s.size();
    data->slot[slot].offset = cur_offset;
    std::copy(s.begin(), s.end(), dst + cur_offset);
    // dst[cur_offset + s.size()] = '\0';
    cur_offset += s.size();
    slot++;
  }
  return data;
}

Vector<std::string_view>::Vector(const std::vector<std::string_view> &vec) {
  uint64_t memSize;
  data = writeInMem(vec, memSize);
  fileSize = memSize;
  is_from_mmap = false;
}

}  // namespace btrblocks
