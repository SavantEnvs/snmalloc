// Known-answer oracle for mayhem/test.sh: prints snmalloc's observable allocator behaviour for
// fixed inputs; test.sh diffs the output against mayhem/oracle.expected.
#include "snmalloc/snmalloc.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

int main()
{
  const size_t sizes[] = {1, 8, 16, 17, 100, 1000, 4096, 5000, 65536, 70000, 1u << 20};
  for (size_t s : sizes)
  {
    void* p = snmalloc::alloc(s);
    char* b = static_cast<char*>(snmalloc::external_pointer<snmalloc::Start>(
      static_cast<char*>(p) + (s - 1)));
    char* e = static_cast<char*>(snmalloc::external_pointer<snmalloc::End>(p));
    std::printf(
      "size %zu: round=%zu alloc_size=%zu owned=%d start_ok=%d span=%zu\n",
      s,
      snmalloc::round_size(s),
      snmalloc::alloc_size(p),
      (int)snmalloc::is_owned(p),
      (int)(b == p),
      (size_t)(e - b));
    snmalloc::dealloc(p, s);
  }

  std::printf("aligned_size(128,160)=%zu\n", snmalloc::aligned_size(128, 160));
  std::printf("aligned_size(64,10)=%zu\n", snmalloc::aligned_size(64, 10));

  for (size_t a : {16, 64, 256, 4096})
  {
    void* p = snmalloc::alloc_aligned(a, 40);
    std::printf(
      "aligned %zu: mod=%zu size_ok=%d\n",
      a,
      (size_t)(reinterpret_cast<uintptr_t>(p) % a),
      (int)(snmalloc::alloc_size(p) >= 40));
    snmalloc::dealloc(p);
  }

  char* z = static_cast<char*>(snmalloc::alloc<snmalloc::Zero>(3000));
  size_t nonzero = 0;
  for (size_t i = 0; i < 3000; i++)
    nonzero += z[i] != 0;
  std::printf("zeroed alloc: nonzero=%zu\n", nonzero);
  snmalloc::dealloc(z);

  char* src = static_cast<char*>(snmalloc::alloc(64));
  char* dst = static_cast<char*>(snmalloc::alloc(64));
  std::memcpy(src, "snmalloc checked memcpy: known answer\0", 39);
  snmalloc::memcpy<true>(dst, src, 39);
  std::printf("memcpy: %s (%d)\n", dst, std::memcmp(src, dst, 39));
  snmalloc::dealloc(src);
  snmalloc::dealloc(dst);
  return 0;
}
