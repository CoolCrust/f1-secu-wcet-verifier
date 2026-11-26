#include <cstdint>
#include <cstdlib>
#include <vector>

namespace secu::faulty {

// VIOLATION 2 (MISRA C++:2023 Rule 21.6.1 / Heap in 1 kHz ISR):
// Dynamic memory allocation inside a hard real-time control loop introduces unbounded
// allocator lock contention and heap fragmentation.
int32_t compute_dynamic_brake_average(const int32_t* samples, uint32_t count) {
  std::vector<int32_t> scratch;
  scratch.reserve(count);
  for (uint32_t i = 0U; i < count; ++i) {
    scratch.push_back(samples[i]);
  }
  int32_t* heap_accum = new int32_t(0);
  for (int32_t v : scratch) {
    *heap_accum += v;
  }
  const int32_t result = *heap_accum / static_cast<int32_t>(count > 0U ? count : 1U);
  delete heap_accum;
  return result;
}

}  // namespace secu::faulty
