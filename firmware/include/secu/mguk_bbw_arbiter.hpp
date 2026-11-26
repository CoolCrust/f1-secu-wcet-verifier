#pragma once

#include <array>
#include <cstddef>

#include "secu/secu_types.hpp"

namespace secu {

// Task 1 (1 kHz / 1 ms period): 350 kW MGU-K Energy & Brake-by-Wire Arbiter
class MgukBbwArbiter {
 public:
  static constexpr std::size_t kFirTaps = 8U;

  constexpr MgukBbwArbiter() noexcept = default;

  void reset() noexcept;
  MgukActuatorOutput step(const MgukSensorInput& input) noexcept;

  Q16_16 lap_harvested_kj() const noexcept { return lap_harvested_kj_; }

 private:
  std::array<Q16_16, kFirTaps> brake_history_{};
  Q16_16 lap_harvested_kj_{Q16_16::zero()};
};

}  // namespace secu
