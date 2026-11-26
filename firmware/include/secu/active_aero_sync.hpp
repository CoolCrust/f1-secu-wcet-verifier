#pragma once

#include <cstdint>

#include "secu/secu_types.hpp"

namespace secu {

// Task 2 (500 Hz / 2 ms period): Active Aerodynamics Sync & Flap Guard
class ActiveAeroController {
 public:
  static constexpr uint32_t kMaxActuatorSubsteps = 6U;

  constexpr ActiveAeroController() noexcept = default;

  void reset() noexcept;
  AeroActuatorOutput step(const AeroSensorInput& input) noexcept;

  bool is_fault_latched() const noexcept { return fault_latched_; }

 private:
  Q16_16 current_front_deg_{Q16_16::zero()};
  Q16_16 current_rear_deg_{Q16_16::zero()};
  uint32_t fault_trip_count_{0U};
  bool fault_latched_{false};
};

}  // namespace secu
