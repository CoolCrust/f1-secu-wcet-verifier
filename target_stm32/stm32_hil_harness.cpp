// Bare-metal ARM Cortex-M4/M7 Hardware-in-the-Loop (HIL) DWT->CYCCNT & UART Emitter
// Compatible with STM32H750VBT6 (400 MHz) and STM32F411CEU6 BlackPill (100 MHz)
#include <cstdint>
#include "secu/mguk_bbw_arbiter.hpp"
#include "secu/active_aero_sync.hpp"
#include "secu/telemetry_framer.hpp"

namespace {
// ARMv7E-M CoreSight DWT & SCB Architectural Registers
constexpr uintptr_t kCoreDebugDemcrAddr = 0xE000EDFCU;
constexpr uintptr_t kDwtCtrlAddr        = 0xE0001000U;
constexpr uintptr_t kDwtCyccntAddr      = 0xE0001004U;
constexpr uintptr_t kScbCcrAddr         = 0xE000ED14U;

inline volatile uint32_t& reg32(uintptr_t addr) noexcept {
  return *reinterpret_cast<volatile uint32_t*>(addr);
}

inline void enable_dwt_and_icache(bool enable_icache) noexcept {
  // Enable TRCENA in CoreDebug->DEMCR, reset and start DWT->CYCCNT
  reg32(kCoreDebugDemcrAddr) |= (1U << 24U);
  reg32(kDwtCyccntAddr) = 0U;
  reg32(kDwtCtrlAddr) |= 1U;

  if (enable_icache) {
    reg32(kScbCcrAddr) |= (1U << 17U);  // SCB_CCR_IC_Msk (Cortex-M7 L1 I-Cache)
  } else {
    reg32(kScbCcrAddr) &= ~(1U << 17U);
  }
  asm volatile("dsb\n\tisb" ::: "memory");
}

inline uint32_t read_cyccnt() noexcept {
  return reg32(kDwtCyccntAddr);
}

// Board-specific UART TX hook (bind to USART1/USART3 TDR on STM32H7/F4)
extern "C" void board_uart_write_byte(uint8_t byte) noexcept;

void uart_print_str(const char* s) noexcept {
  while (*s != '\0') {
    board_uart_write_byte(static_cast<uint8_t>(*s++));
  }
}

void uart_print_u32(uint32_t val) noexcept {
  char buf[12];
  int idx = 0;
  if (val == 0U) {
    board_uart_write_byte('0');
    return;
  }
  while (val > 0U && idx < 11) {
    buf[idx++] = static_cast<char>('0' + (val % 10U));
    val /= 10U;
  }
  while (idx > 0) {
    board_uart_write_byte(static_cast<uint8_t>(buf[--idx]));
  }
}
}  // namespace

extern "C" void secu_run_hardware_hil_loop(bool enable_icache) noexcept {
  enable_dwt_and_icache(enable_icache);

  secu::MgukBbwArbiter mguk{};
  secu::ActiveAeroController aero{};
  secu::TelemetryFramer framer{};

  for (uint32_t tick = 0U; tick < 200U; ++tick) {
    secu::MgukSensorInput m_in{};
    m_in.brake_pedal_q16 = (tick >= 50U && tick < 100U) ? 65536 : 0;
    m_in.vehicle_speed_mps_q16 = 88 * 65536;
    m_in.battery_soc_q16 = 45875;
    m_in.battery_temp_c_q16 = 34 * 65536;
    m_in.lap_harvest_kj_q16 = static_cast<int32_t>(tick * 1200U);
    m_in.overtake_override = (tick >= 100U && tick < 140U);

    const uint32_t c0 = read_cyccnt();
    const auto m_out = mguk.step(m_in);
    const uint32_t t1_cycles = read_cyccnt() - c0;

    uint32_t t2_cycles = 0U;
    secu::AeroCommand a_out{};
    if ((tick % 2U) == 0U) {
      secu::AeroSensorInput a_in{};
      a_in.speed_mps_q16 = m_in.vehicle_speed_mps_q16;
      a_in.steering_angle_deg_q16 = 0;
      a_in.front_flap_feedback_deg_q16 = 16 * 65536;
      a_in.rear_flap_feedback_deg_q16 = (tick >= 140U && tick < 170U) ? (22 * 65536) : (16 * 65536);
      a_in.straight_mode_zone = (tick < 50U);
      a_in.wet_track_flag = (tick >= 170U);

      const uint32_t c1 = read_cyccnt();
      a_out = aero.step(a_in);
      t2_cycles = read_cyccnt() - c1;
    }

    uint32_t t3_cycles = 0U;
    if ((tick % 10U) == 0U) {
      const uint32_t c2 = read_cyccnt();
      (void)framer.pack_and_enqueue(tick, m_out, a_out, m_in.battery_soc_q16);
      t3_cycles = read_cyccnt() - c2;
    }

    const uint32_t total = t1_cycles + t2_cycles + t3_cycles;
    // Wire protocol: DWT,tick,t1,t2,t3,ic_hits,ic_misses,flash_stalls,total
    uart_print_str("DWT,");
    uart_print_u32(tick);
    board_uart_write_byte(',');
    uart_print_u32(t1_cycles);
    board_uart_write_byte(',');
    uart_print_u32(t2_cycles);
    board_uart_write_byte(',');
    uart_print_u32(t3_cycles);
    uart_print_str(",0,0,0,");
    uart_print_u32(total);
    uart_print_str("\r\n");
  }
}
