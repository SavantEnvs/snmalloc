// libFuzzer port of upstream fuzzing/snmalloc-fuzzer.cpp `simple_memcpy`: snmalloc's checked
// memcpy between two snmalloc-owned heap objects must copy every byte.
#include "snmalloc/snmalloc.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
  auto* src = static_cast<char*>(snmalloc::alloc(size));
  auto* dst = static_cast<char*>(snmalloc::alloc(size));
  if (size != 0)
  {
    for (size_t i = 0; i < size; i++)
      src[i] = static_cast<char>(data[i]);
    snmalloc::memcpy<true>(dst, src, size);
    if (std::memcmp(dst, data, size) != 0)
      abort();
  }
  snmalloc::dealloc(src);
  snmalloc::dealloc(dst);
  return 0;
}
