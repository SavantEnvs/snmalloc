// libFuzzer port of upstream `memcpy_with_align_offset`: copy through snmalloc-owned objects
// of fuzzer-chosen alignment, starting at fuzzer-chosen offsets inside them.
#include "snmalloc/snmalloc.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
  // 1 byte each: source alignment exponent, dest alignment exponent; 2 bytes each: offsets.
  if (size < 6)
    return -1;
  size_t src_align = size_t{1} << (data[0] % 7);
  size_t dst_align = size_t{1} << (data[1] % 7);
  size_t src_off = ((size_t{data[2]} << 8) | data[3]) % src_align;
  size_t dst_off = ((size_t{data[4]} << 8) | data[5]) % dst_align;
  const uint8_t* payload = data + 6;
  size_t len = size - 6;

  auto* src_base = static_cast<char*>(snmalloc::alloc_aligned(src_align, len + src_off));
  auto* dst_base = static_cast<char*>(snmalloc::alloc_aligned(dst_align, len + dst_off));
  char* src = src_base + src_off;
  char* dst = dst_base + dst_off;

  snmalloc::memcpy<true>(src, payload, len);
  snmalloc::memcpy<true>(dst, src, len);
  if (len != 0 && std::memcmp(dst, payload, len) != 0)
    abort();

  snmalloc::dealloc(src_base);
  snmalloc::dealloc(dst_base);
  return 0;
}
