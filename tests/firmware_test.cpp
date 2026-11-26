#include <gtest/gtest.h>

#include <array>
#include <cstring>

#include "secu/active_aero_sync.hpp"
#include "secu/fixed_point.hpp"
#include "secu/mguk_bbw_arbiter.hpp"
#include "secu/ring_buffer.hpp"
#include "secu/telemetry_framer.hpp"

using namespace secu;

TEST(FixedPointTest, ArithmeticAndSaturation) {
  constexpr Q16_16 a = Q16_16::from_literal(12.5);
  constexpr Q16_16 b = Q16_16::from_literal(4.0);

  EXPECT_EQ((a + b).to_milli(), 16500);
  EXPECT_EQ((a - b).to_milli(), 8500);
  EXPECT_EQ((a * b).to_milli(), 50000);
  EXPECT_EQ((a / b).to_milli(), 3125);

  // Overflow saturation check
  constexpr Q16_16 huge = Q16_16::from_literal(30000.0);
  const Q16_16 saturated = huge + huge;
  EXPECT_EQ(saturated.raw(), std::numeric_limits<int32_t>::max());
}

TEST(MgukBbwArbiterTest, BrakeTorqueSplitAndHarvestCap) {
  MgukBbwArbiter arbiter;
  arbiter.reset();

  MgukSensorInput input{};
  input.brake_pedal_permille = 1000;  // Full braking demand (1200 Nm after FIR settles)
  input.rear_wheel_omega_rads = Q16_16::from_literal(200.0);
  input.battery_soc_permille = 450;
  input.overtake_mode_active = false;

  // Pump 8 ticks to fill the 8-tap FIR filter
  MgukActuatorOutput out{};
  for (int i = 0; i < 8; ++i) {
    out = arbiter.step(input);
  }

  // Total requested rear brake torque should reach 1200 Nm
  EXPECT_NEAR(out.total_rear_torque_req_nm.to_milli(), 1200000, 100);
  // MGU-K torque is capped at 420 Nm (which at 200 rad/s is 84 kW < 350 kW)
  EXPECT_NEAR(out.mguk_regen_torque_nm.to_milli(), 420000, 100);
  // Hydraulic rear brake supplies the exact residual: 1200 - 420 = 780 Nm
  EXPECT_NEAR(out.hydraulic_rear_torque_nm.to_milli(), 780000, 100);
  EXPECT_NEAR(out.mguk_regen_power_kw.to_milli(), 84000, 100);

  // Verify 8.0 MJ (8000 kJ) per-lap harvesting ceiling vs. 8.5 MJ Overtake Mode allowance
  input.rear_wheel_omega_rads = Q16_16::from_literal(250.0);  // 420 Nm * 250 = 105 kW
  for (int tick = 0; tick < 80000; ++tick) {
    out = arbiter.step(input);
  }
  EXPECT_NEAR(out.lap_harvested_kj.to_milli(), 8000000, 500);
  EXPECT_NE(out.fault_bits & static_cast<uint16_t>(FaultFlags::LapHarvestCapReached), 0U);
  EXPECT_EQ(out.mguk_regen_torque_nm.raw(), 0);

  // Enabling Overtake Mode unlocks +0.5 MJ (up to 8500 kJ)
  input.overtake_mode_active = true;
  for (int tick = 0; tick < 10000; ++tick) {
    out = arbiter.step(input);
  }
  EXPECT_NEAR(out.lap_harvested_kj.to_milli(), 8500000, 500);
}

TEST(ActiveAeroTest, ModesAndAsymmetryEmergencySnapClose) {
  ActiveAeroController aero;
  aero.reset();

  AeroSensorInput in{};
  in.requested_mode = AeroMode::Straight;
  in.front_left_flap_deg = Q16_16::from_literal(10.0);
  in.front_right_flap_deg = Q16_16::from_literal(10.5);
  in.rear_left_flap_deg = Q16_16::from_literal(15.0);
  in.rear_right_flap_deg = Q16_16::from_literal(15.2);
  in.requested_substeps = 6U;

  const AeroActuatorOutput straight_out = aero.step(in);
  EXPECT_EQ(straight_out.active_mode, AeroMode::Straight);
  // 6 substeps * 1.5 deg/step = 9.0 deg front and rear
  EXPECT_EQ(straight_out.front_flap_cmd_deg.to_milli(), 9000);
  EXPECT_EQ(straight_out.rear_flap_cmd_deg.to_milli(), 9000);

  // Wet-weather single-flap fallback locks front flap to 0 deg
  in.track_wet_condition = true;
  const AeroActuatorOutput wet_out = aero.step(in);
  EXPECT_EQ(wet_out.active_mode, AeroMode::WetFallback);
  EXPECT_EQ(wet_out.front_flap_cmd_deg.to_milli(), 0);

  // Inject > 2.0 deg asymmetry on rear wing -> Emergency snap-close latch
  in.track_wet_condition = false;
  in.rear_left_flap_deg = Q16_16::from_literal(18.0);
  in.rear_right_flap_deg = Q16_16::from_literal(14.5);  // 3.5 deg divergence > 2.0 deg
  const AeroActuatorOutput fault_out = aero.step(in);
  EXPECT_EQ(fault_out.active_mode, AeroMode::EmergencyClosed);
  EXPECT_EQ(fault_out.rear_flap_cmd_deg.to_milli(), 0);
  EXPECT_EQ(fault_out.fault_trip_count, 1U);
  EXPECT_NE(fault_out.fault_bits & static_cast<uint16_t>(FaultFlags::AeroAsymmetryLatched), 0U);
}

TEST(TelemetryFramerTest, Crc16StandardVectorAndRingBuffer) {
  // Standard CRC-16-CCITT (False) check on ASCII "123456789" == 0x29B1
  const char* vector = "123456789";
  const uint16_t crc = TelemetryFramer::compute_crc16_ccitt(
      reinterpret_cast<const uint8_t*>(vector), std::strlen(vector));
  EXPECT_EQ(crc, 0x29B1U);

  TelemetryFramer framer;
  framer.reset();

  MgukActuatorOutput mguk{};
  mguk.mguk_regen_power_kw = Q16_16::from_literal(350.0);
  mguk.lap_harvested_kj = Q16_16::from_literal(4250.0);

  AeroActuatorOutput aero{};
  aero.active_mode = AeroMode::Straight;
  aero.front_flap_cmd_deg = Q16_16::from_literal(18.0);
  aero.rear_flap_cmd_deg = Q16_16::from_literal(28.0);

  for (int i = 0; i < 18; ++i) {
    const TelemetryFrame f = framer.pack_and_enqueue(mguk, aero);
    EXPECT_EQ(f.bytes[0], 0xF1U);
    EXPECT_EQ(f.bytes[1], 0x26U);
    const uint16_t expected_crc =
        TelemetryFramer::compute_crc16_ccitt(f.bytes.data(), kTelemetryPayloadBytes);
    const uint16_t packed_crc = static_cast<uint16_t>(
        (static_cast<uint16_t>(f.bytes[24]) << 8U) | f.bytes[25]);
    EXPECT_EQ(packed_crc, expected_crc);
  }

  // Capacity is 16; 18 pushes must record 2 deterministic overflows
  EXPECT_EQ(framer.queued_frames(), 16U);
  EXPECT_EQ(framer.overflow_count(), 2U);
}
