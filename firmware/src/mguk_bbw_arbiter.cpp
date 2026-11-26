#include "secu/mguk_bbw_arbiter.hpp"

namespace secu {

namespace {

constexpr std::array<Q16_16, MgukBbwArbiter::kFirTaps> kFirCoeffs = {
    Q16_16::from_literal(0.0625), Q16_16::from_literal(0.1000),
    Q16_16::from_literal(0.1500), Q16_16::from_literal(0.1875),
    Q16_16::from_literal(0.1875), Q16_16::from_literal(0.1500),
    Q16_16::from_literal(0.1000), Q16_16::from_literal(0.0625)
};

constexpr Q16_16 kMaxRearBrakeDemandNm = Q16_16::from_literal(1200.0);
constexpr Q16_16 kControlPeriodSec = Q16_16::from_literal(0.001);
constexpr Q16_16 kKwToWattsScale = Q16_16::from_literal(1000.0);
constexpr int32_t kMaxBatterySocPermille = 950;

}  // namespace

void MgukBbwArbiter::reset() noexcept {
  for (std::size_t i = 0U; i < kFirTaps; ++i) {
    brake_history_[i] = Q16_16::zero();
  }
  lap_harvested_kj_ = Q16_16::zero();
}

MgukActuatorOutput MgukBbwArbiter::step(const MgukSensorInput& input) noexcept {
  MgukActuatorOutput out{};

  if (input.new_lap_beacon) {
    lap_harvested_kj_ = Q16_16::zero();
  }

  int32_t clamped_pedal = input.brake_pedal_permille;
  if (clamped_pedal < 0) {
    clamped_pedal = 0;
  } else if (clamped_pedal > 1000) {
    clamped_pedal = 1000;
  }

  for (std::size_t idx = kFirTaps - 1U; idx > 0U; --idx) {
    brake_history_[idx] = brake_history_[idx - 1U];
  }
  brake_history_[0U] = Q16_16::from_milli(clamped_pedal);

  Q16_16 filtered_pedal_ratio = Q16_16::zero();
  for (std::size_t idx = 0U; idx < kFirTaps; ++idx) {
    filtered_pedal_ratio = filtered_pedal_ratio + (brake_history_[idx] * kFirCoeffs[idx]);
  }

  const Q16_16 total_rear_torque = filtered_pedal_ratio * kMaxRearBrakeDemandNm;
  out.total_rear_torque_req_nm = total_rear_torque;

  const Q16_16 active_harvest_limit_kj =
      input.overtake_mode_active ? (kBaseLapHarvestLimitKj + kOvertakeBonusKj)
                                 : kBaseLapHarvestLimitKj;

  bool regen_inhibited = false;
  if (input.rear_wheel_omega_rads < kMinRegenWheelOmegaRads) {
    out.fault_bits = out.fault_bits | FaultFlags::LowWheelSpeedRegenCut;
    regen_inhibited = true;
  }
  if (input.battery_soc_permille >= kMaxBatterySocPermille) {
    out.fault_bits = out.fault_bits | FaultFlags::BatteryHighSocDerate;
    regen_inhibited = true;
  }
  if (lap_harvested_kj_ >= active_harvest_limit_kj) {
    out.fault_bits = out.fault_bits | FaultFlags::LapHarvestCapReached;
    regen_inhibited = true;
  }

  if (regen_inhibited || total_rear_torque <= Q16_16::zero()) {
    out.mguk_regen_torque_nm = Q16_16::zero();
    out.mguk_regen_power_kw = Q16_16::zero();
    out.hydraulic_rear_torque_nm = total_rear_torque;
    out.lap_harvested_kj = lap_harvested_kj_;
    return out;
  }

  const Q16_16 power_limited_torque =
      Q16_16::mul_div(kMaxMgukPowerKw, kKwToWattsScale, input.rear_wheel_omega_rads);
  const Q16_16 max_regen_torque = Q16_16::min(power_limited_torque, kMaxMgukTorqueNm);
  Q16_16 regen_torque = Q16_16::min(total_rear_torque, max_regen_torque);

  Q16_16 regen_power_kw =
      Q16_16::mul_div(regen_torque, input.rear_wheel_omega_rads, kKwToWattsScale);
  if (regen_power_kw > kMaxMgukPowerKw) {
    regen_power_kw = kMaxMgukPowerKw;
  }

  const Q16_16 remaining_kj = active_harvest_limit_kj - lap_harvested_kj_;
  const Q16_16 step_energy_kj = regen_power_kw * kControlPeriodSec;

  if (step_energy_kj > remaining_kj) {
    regen_power_kw = remaining_kj / kControlPeriodSec;
    regen_torque =
        Q16_16::mul_div(regen_power_kw, kKwToWattsScale, input.rear_wheel_omega_rads);
    lap_harvested_kj_ = active_harvest_limit_kj;
    out.fault_bits = out.fault_bits | FaultFlags::LapHarvestCapReached;
  } else {
    lap_harvested_kj_ = lap_harvested_kj_ + step_energy_kj;
  }

  out.mguk_regen_torque_nm = regen_torque;
  out.mguk_regen_power_kw = regen_power_kw;
  out.hydraulic_rear_torque_nm = total_rear_torque - regen_torque;
  out.lap_harvested_kj = lap_harvested_kj_;
  return out;
}

}  // namespace secu
