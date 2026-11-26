#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "secu/fixed_point.hpp"

namespace secu {

// 2026 FIA Technical Regulations Constants
inline constexpr Q16_16 kMaxMgukPowerKw = Q16_16::from_literal(350.0);
inline constexpr Q16_16 kMaxMgukTorqueNm = Q16_16::from_literal(420.0);
inline constexpr Q16_16 kBaseLapHarvestLimitKj = Q16_16::from_literal(8000.0);  // 8.0 MJ
inline constexpr Q16_16 kOvertakeBonusKj = Q16_16::from_literal(500.0);         // +0.5 MJ
inline constexpr Q16_16 kMinRegenWheelOmegaRads = Q16_16::from_literal(15.0);
inline constexpr Q16_16 kMaxAeroAsymmetryDeg = Q16_16::from_literal(2.0);

enum class AeroMode : uint8_t {
  Corner = 0U,
  Straight = 1U,
  WetFallback = 2U,
  EmergencyClosed = 3U
};

enum class FaultFlags : uint16_t {
  None = 0x0000U,
  LapHarvestCapReached = 0x0001U,
  BatteryHighSocDerate = 0x0002U,
  LowWheelSpeedRegenCut = 0x0004U,
  AeroAsymmetryLatched = 0x0008U,
  TelemetryBufferOverflow = 0x0010U
};

constexpr uint16_t operator|(uint16_t lhs, FaultFlags rhs) noexcept {
  return static_cast<uint16_t>(lhs | static_cast<uint16_t>(rhs));
}

// Task 1 (1 kHz / 1 ms): MGU-K Energy & Brake-by-Wire Arbiter I/O
struct MgukSensorInput {
  int32_t brake_pedal_permille{0};     // [0..1000]
  Q16_16 rear_wheel_omega_rads{};      // [0..350] rad/s
  int32_t battery_soc_permille{500};   // [0..1000]
  bool overtake_mode_active{false};    // 2026 Overtake Mode (+0.5 MJ)
  bool new_lap_beacon{false};          // Start/finish line reset pulse
};

struct MgukActuatorOutput {
  Q16_16 total_rear_torque_req_nm{};
  Q16_16 mguk_regen_torque_nm{};
  Q16_16 hydraulic_rear_torque_nm{};
  Q16_16 mguk_regen_power_kw{};
  Q16_16 lap_harvested_kj{};
  uint16_t fault_bits{0U};
};

// Task 2 (500 Hz / 2 ms): Active Aerodynamics Synchronization I/O
struct AeroSensorInput {
  AeroMode requested_mode{AeroMode::Corner};
  Q16_16 front_left_flap_deg{};
  Q16_16 front_right_flap_deg{};
  Q16_16 rear_left_flap_deg{};
  Q16_16 rear_right_flap_deg{};
  bool track_wet_condition{false};
  uint32_t requested_substeps{4U};
};

struct AeroActuatorOutput {
  AeroMode active_mode{AeroMode::Corner};
  Q16_16 front_flap_cmd_deg{};
  Q16_16 rear_flap_cmd_deg{};
  uint32_t executed_substeps{0U};
  uint32_t fault_trip_count{0U};
  uint16_t fault_bits{0U};
};

// Task 3 (100 Hz / 10 ms): FIA Telemetry Packet (24-byte payload + 2-byte CRC16 = 26 bytes)
inline constexpr std::size_t kTelemetryPayloadBytes = 24U;
inline constexpr std::size_t kTelemetryFrameBytes = 26U;

struct TelemetryFrame {
  std::array<uint8_t, kTelemetryFrameBytes> bytes{};
};

}  // namespace secu
