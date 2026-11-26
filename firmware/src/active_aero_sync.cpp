#include "secu/active_aero_sync.hpp"
#include "secu/wcet_annotations.hpp"

namespace secu {

namespace {

constexpr Q16_16 kCornerFrontDeg = Q16_16::from_literal(0.0);
constexpr Q16_16 kCornerRearDeg = Q16_16::from_literal(0.0);

constexpr Q16_16 kStraightFrontDeg = Q16_16::from_literal(18.0);
constexpr Q16_16 kStraightRearDeg = Q16_16::from_literal(28.0);

// Wet-weather single-flap fallback: front flap locked closed for front-axle grip
constexpr Q16_16 kWetFrontDeg = Q16_16::from_literal(0.0);
constexpr Q16_16 kWetRearDeg = Q16_16::from_literal(12.0);

constexpr Q16_16 kSlewStepDeg = Q16_16::from_literal(1.5);
constexpr Q16_16 kEmergencySnapStepDeg = Q16_16::from_literal(6.0);

constexpr Q16_16 step_toward(Q16_16 current, Q16_16 target, Q16_16 max_delta) noexcept {
  const Q16_16 diff = target - current;
  if (diff.abs() <= max_delta) {
    return target;
  }
  return (diff > Q16_16::zero()) ? (current + max_delta) : (current - max_delta);
}

}  // namespace

void ActiveAeroController::reset() noexcept {
  current_front_deg_ = Q16_16::zero();
  current_rear_deg_ = Q16_16::zero();
  fault_trip_count_ = 0U;
  fault_latched_ = false;
}

AeroActuatorOutput ActiveAeroController::step(const AeroSensorInput& input) noexcept {
  AeroActuatorOutput out{};

  // 1. Asymmetric flap divergence guard (> 2.0 degrees left vs. right triggers snap-close)
  const Q16_16 front_asymmetry =
      (input.front_left_flap_deg - input.front_right_flap_deg).abs();
  const Q16_16 rear_asymmetry =
      (input.rear_left_flap_deg - input.rear_right_flap_deg).abs();

  if (front_asymmetry > kMaxAeroAsymmetryDeg || rear_asymmetry > kMaxAeroAsymmetryDeg) {
    if (!fault_latched_) {
      fault_latched_ = true;
      ++fault_trip_count_;
    }
  } else if (input.requested_mode == AeroMode::Corner &&
             current_front_deg_ == kCornerFrontDeg &&
             current_rear_deg_ == kCornerRearDeg) {
    // Clear fault latch once flaps are physically verified closed and driver requests Corner Mode
    fault_latched_ = false;
  }

  Q16_16 target_front = kCornerFrontDeg;
  Q16_16 target_rear = kCornerRearDeg;
  Q16_16 slew_rate = kSlewStepDeg;
  AeroMode effective_mode = AeroMode::Corner;

  // 2. Mutually exclusive operating mode arbitration (IPET infeasible-path group)
  if (fault_latched_) {
    WCET_PATH_GROUP("aero_mode", "emergency");
    effective_mode = AeroMode::EmergencyClosed;
    target_front = kCornerFrontDeg;
    target_rear = kCornerRearDeg;
    slew_rate = kEmergencySnapStepDeg;
    out.fault_bits = out.fault_bits | FaultFlags::AeroAsymmetryLatched;
  } else if (input.track_wet_condition || input.requested_mode == AeroMode::WetFallback) {
    WCET_PATH_GROUP("aero_mode", "wet");
    effective_mode = AeroMode::WetFallback;
    target_front = kWetFrontDeg;
    target_rear = kWetRearDeg;
  } else if (input.requested_mode == AeroMode::Straight) {
    WCET_PATH_GROUP("aero_mode", "straight");
    effective_mode = AeroMode::Straight;
    target_front = kStraightFrontDeg;
    target_rear = kStraightRearDeg;
  } else {
    WCET_PATH_GROUP("aero_mode", "corner");
    effective_mode = AeroMode::Corner;
    target_front = kCornerFrontDeg;
    target_rear = kCornerRearDeg;
  }

  // 3. Data-dependent micro-step servo convergence loop bounded by kMaxActuatorSubsteps
  uint32_t bounded_steps = input.requested_substeps;
  if (bounded_steps > kMaxActuatorSubsteps) {
    bounded_steps = kMaxActuatorSubsteps;
  }

  uint32_t steps_taken = 0U;
  WCET_MAX_ITER(6)
  while (steps_taken < bounded_steps) {
    if (current_front_deg_ == target_front && current_rear_deg_ == target_rear) {
      break;
    }
    current_front_deg_ = step_toward(current_front_deg_, target_front, slew_rate);
    current_rear_deg_ = step_toward(current_rear_deg_, target_rear, slew_rate);
    ++steps_taken;
  }

  out.active_mode = effective_mode;
  out.front_flap_cmd_deg = current_front_deg_;
  out.rear_flap_cmd_deg = current_rear_deg_;
  out.executed_substeps = steps_taken;
  out.fault_trip_count = fault_trip_count_;
  return out;
}

}  // namespace secu
