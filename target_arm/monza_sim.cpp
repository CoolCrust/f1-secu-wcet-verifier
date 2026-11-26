#include <cstdint>

#include "secu/active_aero_sync.hpp"
#include "secu/mguk_bbw_arbiter.hpp"
#include "secu/telemetry_framer.hpp"

namespace {

// CMSDK APB UART0 on ARM MPS2-AN385
constexpr uintptr_t kUart0Base = 0x40004000U;
volatile uint32_t& kUart0Data = *reinterpret_cast<volatile uint32_t*>(kUart0Base + 0x00U);
volatile uint32_t& kUart0State = *reinterpret_cast<volatile uint32_t*>(kUart0Base + 0x04U);
volatile uint32_t& kUart0Ctrl = *reinterpret_cast<volatile uint32_t*>(kUart0Base + 0x08U);

// ARMv7-M CoreDebug, DWT, and SysTick Registers
volatile uint32_t& kDemcr = *reinterpret_cast<volatile uint32_t*>(0xE000EDFCU);
volatile uint32_t& kDwtCtrl = *reinterpret_cast<volatile uint32_t*>(0xE0001000U);
volatile uint32_t& kDwtCyccnt = *reinterpret_cast<volatile uint32_t*>(0xE0001004U);
volatile uint32_t& kSystickCtrl = *reinterpret_cast<volatile uint32_t*>(0xE000E010U);
volatile uint32_t& kSystickLoad = *reinterpret_cast<volatile uint32_t*>(0xE000E014U);
volatile uint32_t& kSystickVal = *reinterpret_cast<volatile uint32_t*>(0xE000E018U);

void uart_init() noexcept {
  kUart0Ctrl = 0x01U;  // Enable TX
}

void uart_putc(char c) noexcept {
  while ((kUart0State & 0x01U) != 0U) {
  }
  kUart0Data = static_cast<uint32_t>(static_cast<uint8_t>(c));
}

void uart_puts(const char* s) noexcept {
  while (*s != '\0') {
    uart_putc(*s++);
  }
}

void uart_put_u32(uint32_t val) noexcept {
  char buf[12];
  uint32_t idx = 0U;
  if (val == 0U) {
    uart_putc('0');
    return;
  }
  while (val > 0U) {
    buf[idx++] = static_cast<char>('0' + (val % 10U));
    val /= 10U;
  }
  while (idx > 0U) {
    uart_putc(buf[--idx]);
  }
}

void uart_put_i32(int32_t val) noexcept {
  if (val < 0) {
    uart_putc('-');
    const uint32_t mag = static_cast<uint32_t>(-(val + 1)) + 1U;
    uart_put_u32(mag);
    return;
  }
  uart_put_u32(static_cast<uint32_t>(val));
}

void cycle_counter_init() noexcept {
  kDemcr |= (1U << 24U);  // TRCENA
  kDwtCyccnt = 0U;
  kDwtCtrl |= 1U;         // CYCCNTENA

  kSystickLoad = 0x00FFFFFFU;
  kSystickVal = 0U;
  kSystickCtrl = 0x05U;   // Enable processor-clock down-counter
}

struct CycleSnapshot {
  uint32_t dwt;
  uint32_t systick;
};

inline CycleSnapshot read_cycles() noexcept {
  return CycleSnapshot{kDwtCyccnt, kSystickVal & 0x00FFFFFFU};
}

inline uint32_t elapsed_cycles(CycleSnapshot start, CycleSnapshot end) noexcept {
  const uint32_t dwt_diff = end.dwt - start.dwt;
  if (dwt_diff > 0U) {
    return dwt_diff;
  }
  return (start.systick - end.systick) & 0x00FFFFFFU;
}

}  // namespace

int main() {
  uart_init();
  cycle_counter_init();

  secu::MgukBbwArbiter mguk;
  secu::ActiveAeroController aero;
  secu::TelemetryFramer framer;

  mguk.reset();
  aero.reset();
  framer.reset();

  uart_puts(
      "tick,segment,task1_cycles,task2_cycles,task3_cycles,total_tick_cycles,"
      "mguk_power_kw_milli,lap_harvest_kj_milli,aero_mode,fault_bits\n");

  secu::AeroActuatorOutput last_aero{};
  uint32_t max_t1 = 0U;
  uint32_t max_t2 = 0U;
  uint32_t max_t3 = 0U;

  for (uint32_t tick = 0U; tick < 200U; ++tick) {
    secu::MgukSensorInput mguk_in{};
    secu::AeroSensorInput aero_in{};
    const char* segment = "PIT_STRAIGHT";

    mguk_in.new_lap_beacon = (tick == 0U);
    mguk_in.battery_soc_permille = 520;
    aero_in.requested_substeps = 6U;

    if (tick < 50U) {
      // Sector 1: Main Straight at 340 km/h (Straight Mode Active Aero)
      segment = "PIT_STRAIGHT";
      mguk_in.brake_pedal_permille = 0;
      mguk_in.rear_wheel_omega_rads = secu::Q16_16::from_int(310);
      aero_in.requested_mode = secu::AeroMode::Straight;
      aero_in.front_left_flap_deg = secu::Q16_16::from_int(18);
      aero_in.front_right_flap_deg = secu::Q16_16::from_int(18);
      aero_in.rear_left_flap_deg = secu::Q16_16::from_int(28);
      aero_in.rear_right_flap_deg = secu::Q16_16::from_int(28);
    } else if (tick < 100U) {
      // Turn 1 Variante del Rettifilo Heavy Braking + 350 kW MGU-K Harvesting
      segment = "T1_HEAVY_BRAKE";
      mguk_in.brake_pedal_permille = 980;
      const int32_t omega = 310 - static_cast<int32_t>((tick - 50U) * 4U);
      mguk_in.rear_wheel_omega_rads = secu::Q16_16::from_int(omega);
      aero_in.requested_mode = secu::AeroMode::Corner;
      aero_in.front_left_flap_deg = secu::Q16_16::from_int(2);
      aero_in.front_right_flap_deg = secu::Q16_16::from_int(2);
      aero_in.rear_left_flap_deg = secu::Q16_16::from_int(4);
      aero_in.rear_right_flap_deg = secu::Q16_16::from_int(4);
    } else if (tick < 140U) {
      // Curva Grande & Seconda Variante: Overtake Mode (+0.5 MJ energy limit)
      segment = "OVERTAKE_BOOST";
      mguk_in.brake_pedal_permille = 850;
      mguk_in.rear_wheel_omega_rads = secu::Q16_16::from_int(240);
      mguk_in.overtake_mode_active = true;
      aero_in.requested_mode = secu::AeroMode::Straight;
      aero_in.front_left_flap_deg = secu::Q16_16::from_int(18);
      aero_in.front_right_flap_deg = secu::Q16_16::from_int(18);
      aero_in.rear_left_flap_deg = secu::Q16_16::from_int(28);
      aero_in.rear_right_flap_deg = secu::Q16_16::from_int(28);
    } else if (tick < 170U) {
      // Lesmo Approach: Inject > 2.0 deg flap divergence -> Emergency Snap-Close
      segment = "AERO_FAULT_SNAP";
      mguk_in.brake_pedal_permille = 600;
      mguk_in.rear_wheel_omega_rads = secu::Q16_16::from_int(180);
      aero_in.requested_mode = secu::AeroMode::Corner;
      if (tick < 150U) {
        aero_in.front_left_flap_deg = secu::Q16_16::from_int(10);
        aero_in.front_right_flap_deg = secu::Q16_16::from_int(10);
        aero_in.rear_left_flap_deg = secu::Q16_16::from_int(22);
        aero_in.rear_right_flap_deg = secu::Q16_16::from_literal(18.2);  // 3.8 deg > 2.0 deg
      } else {
        aero_in.front_left_flap_deg = secu::Q16_16::from_int(0);
        aero_in.front_right_flap_deg = secu::Q16_16::from_int(0);
        aero_in.rear_left_flap_deg = secu::Q16_16::from_int(0);
        aero_in.rear_right_flap_deg = secu::Q16_16::from_int(0);
      }
    } else {
      // Parabolica Wet-Weather Single-Flap Fallback
      segment = "PARABOLICA_WET";
      mguk_in.brake_pedal_permille = 450;
      mguk_in.rear_wheel_omega_rads = secu::Q16_16::from_int(195);
      aero_in.track_wet_condition = true;
      aero_in.requested_mode = secu::AeroMode::WetFallback;
      aero_in.front_left_flap_deg = secu::Q16_16::from_int(0);
      aero_in.front_right_flap_deg = secu::Q16_16::from_int(0);
      aero_in.rear_left_flap_deg = secu::Q16_16::from_int(12);
      aero_in.rear_right_flap_deg = secu::Q16_16::from_int(12);
    }

    // Task 1 (1 kHz): Runs every 1 ms tick
    const CycleSnapshot t1_s = read_cycles();
    const secu::MgukActuatorOutput mguk_out = mguk.step(mguk_in);
    const CycleSnapshot t1_e = read_cycles();
    const uint32_t c1 = elapsed_cycles(t1_s, t1_e);
    if (c1 > max_t1) {
      max_t1 = c1;
    }

    // Task 2 (500 Hz): Runs every 2nd tick
    uint32_t c2 = 0U;
    if ((tick % 2U) == 0U) {
      const CycleSnapshot t2_s = read_cycles();
      last_aero = aero.step(aero_in);
      const CycleSnapshot t2_e = read_cycles();
      c2 = elapsed_cycles(t2_s, t2_e);
      if (c2 > max_t2) {
        max_t2 = c2;
      }
    }

    // Task 3 (100 Hz): Runs every 10th tick
    uint32_t c3 = 0U;
    if ((tick % 10U) == 0U) {
      const CycleSnapshot t3_s = read_cycles();
      const secu::TelemetryFrame frame = framer.pack_and_enqueue(mguk_out, last_aero);
      const CycleSnapshot t3_e = read_cycles();
      c3 = elapsed_cycles(t3_s, t3_e);
      if (c3 > max_t3) {
        max_t3 = c3;
      }
      secu::TelemetryFrame drained{};
      if (framer.queued_frames() > 8U) {
        (void)framer.pop_frame(drained);
      }
      (void)frame;
    }

    const uint32_t total_cycles = c1 + c2 + c3;
    const uint16_t combined_faults =
        static_cast<uint16_t>(mguk_out.fault_bits | last_aero.fault_bits);

    uart_put_u32(tick);
    uart_putc(',');
    uart_puts(segment);
    uart_putc(',');
    uart_put_u32(c1);
    uart_putc(',');
    uart_put_u32(c2);
    uart_putc(',');
    uart_put_u32(c3);
    uart_putc(',');
    uart_put_u32(total_cycles);
    uart_putc(',');
    uart_put_i32(mguk_out.mguk_regen_power_kw.to_milli());
    uart_putc(',');
    uart_put_i32(mguk_out.lap_harvested_kj.to_milli());
    uart_putc(',');
    uart_put_u32(static_cast<uint32_t>(last_aero.active_mode));
    uart_putc(',');
    uart_put_u32(combined_faults);
    uart_putc('\n');
  }

  uart_puts("# SUMMARY max_t1=");
  uart_put_u32(max_t1);
  uart_puts(" max_t2=");
  uart_put_u32(max_t2);
  uart_puts(" max_t3=");
  uart_put_u32(max_t3);
  uart_putc('\n');

  return 0;
}
