// libFuzzer port of upstream `snmalloc_random_walk`: a fuzzer-driven sequence of
// alloc(zeroed) / alloc(uninit) / free / verify / refill operations against the snmalloc
// allocator, checking that live objects are never corrupted and allocator metadata agrees.
#include "snmalloc/snmalloc.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace
{
  constexpr size_t size_limit = 16384;
  constexpr size_t max_live = 512;

  struct Live
  {
    char filler;
    char* ptr;
    size_t size;
  };

  void check(const Live& l)
  {
    for (size_t i = 0; i < l.size; i++)
      if (l.ptr[i] != l.filler)
        abort();
  }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
  std::vector<Live> live;
  // Each event: 1 byte kind, 2 bytes size/index, 1 byte filler.
  for (size_t pos = 0; pos + 4 <= size; pos += 4)
  {
    unsigned kind = data[pos] % 5;
    size_t arg = (size_t{data[pos + 1]} << 8) | data[pos + 2];
    char filler = static_cast<char>(data[pos + 3]);
    switch (kind)
    {
      case 0:
      case 1:
      {
        if (live.size() >= max_live)
          break;
        size_t n = arg % size_limit;
        char* p = kind == 0 ? static_cast<char*>(snmalloc::alloc<snmalloc::Zero>(n))
                            : static_cast<char*>(snmalloc::alloc<snmalloc::Uninit>(n));
        if (!snmalloc::is_owned(p) || snmalloc::alloc_size(p) < n ||
            snmalloc::external_pointer(p) != p)
          abort();
        char fill = kind == 0 ? 0 : filler;
        for (size_t i = 0; i < n; i++)
          p[i] = fill;
        live.push_back({fill, p, n});
        break;
      }
      case 2:
      {
        if (live.empty())
          break;
        size_t idx = arg % live.size();
        check(live[idx]);
        snmalloc::dealloc(live[idx].ptr, live[idx].size);
        live.erase(live.begin() + static_cast<std::ptrdiff_t>(idx));
        break;
      }
      case 3:
        for (auto& l : live)
          check(l);
        break;
      case 4:
      {
        if (live.empty())
          break;
        auto& l = live[arg % live.size()];
        for (size_t i = 0; i < l.size; i++)
          l.ptr[i] = filler;
        l.filler = filler;
        break;
      }
    }
  }
  for (auto& l : live)
  {
    check(l);
    snmalloc::dealloc(l.ptr);
  }
  return 0;
}
