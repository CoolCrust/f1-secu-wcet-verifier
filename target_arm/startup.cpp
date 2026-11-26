#include <cstddef>
#include <cstdint>

extern "C" {
extern uint32_t _estack;
extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;
extern void (*__init_array_start[])();
extern void (*__init_array_end[])();

int main();
void Reset_Handler();
void Default_Handler();

__attribute__((used, optimize("no-tree-loop-distribute-patterns")))
void* memset(void* dest, int ch, std::size_t count) {
  auto* d = static_cast<unsigned char*>(dest);
  const auto val = static_cast<unsigned char>(ch);
  for (std::size_t i = 0U; i < count; ++i) {
    d[i] = val;
  }
  return dest;
}

__attribute__((used, optimize("no-tree-loop-distribute-patterns")))
void* memcpy(void* dest, const void* src, std::size_t count) {
  auto* d = static_cast<unsigned char*>(dest);
  const auto* s = static_cast<const unsigned char*>(src);
  for (std::size_t i = 0U; i < count; ++i) {
    d[i] = s[i];
  }
  return dest;
}

__attribute__((used, optimize("no-tree-loop-distribute-patterns")))
void* memmove(void* dest, const void* src, std::size_t count) {
  auto* d = static_cast<unsigned char*>(dest);
  const auto* s = static_cast<const unsigned char*>(src);
  if (d < s) {
    for (std::size_t i = 0U; i < count; ++i) {
      d[i] = s[i];
    }
  } else if (d > s) {
    for (std::size_t i = count; i > 0U; --i) {
      d[i - 1U] = s[i - 1U];
    }
  }
  return dest;
}

__attribute__((used, optimize("no-tree-loop-distribute-patterns")))
int memcmp(const void* lhs, const void* rhs, std::size_t count) {
  const auto* a = static_cast<const unsigned char*>(lhs);
  const auto* b = static_cast<const unsigned char*>(rhs);
  for (std::size_t i = 0U; i < count; ++i) {
    if (a[i] != b[i]) {
      return (a[i] < b[i]) ? -1 : 1;
    }
  }
  return 0;
}
}

__attribute__((noreturn)) void qemu_semihosting_exit(uint32_t status_code) noexcept {
  register uint32_t r0 asm("r0") = 0x18U;  // SYS_EXIT
  register uint32_t r1 asm("r1") = (status_code == 0U) ? 0x20026U : 0x20024U;
  asm volatile("bkpt #0xAB" : : "r"(r0), "r"(r1) : "memory");
  while (true) {
  }
}

extern "C" void Default_Handler() {
  qemu_semihosting_exit(1U);
}

extern "C" void Reset_Handler() {
  uint32_t* src = &_sidata;
  uint32_t* dst = &_sdata;
  while (dst < &_edata) {
    *dst++ = *src++;
  }

  uint32_t* bss = &_sbss;
  while (bss < &_ebss) {
    *bss++ = 0U;
  }

  for (void (**ctor)() = __init_array_start; ctor < __init_array_end; ++ctor) {
    (*ctor)();
  }

  const int rc = main();
  qemu_semihosting_exit(static_cast<uint32_t>(rc));
}

using IsrFn = void (*)();

__attribute__((section(".isr_vector"), used))
const IsrFn kVectorTable[16] = {
    reinterpret_cast<IsrFn>(&_estack),
    Reset_Handler,
    Default_Handler,  // NMI
    Default_Handler,  // HardFault
    Default_Handler,  // MemManage
    Default_Handler,  // BusFault
    Default_Handler,  // UsageFault
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    Default_Handler,  // SVCall
    Default_Handler,  // DebugMon
    nullptr,
    Default_Handler,  // PendSV
    Default_Handler   // SysTick
};
