#pragma once

#include <cstddef>
#include <cstdint>

#include "secu/ring_buffer.hpp"
#include "secu/secu_types.hpp"

namespace secu {

// Task 3 (100 Hz / 10 ms period): FIA Telemetry & Fault Packet Framer
class TelemetryFramer {
 public:
  static constexpr std::size_t kQueueCapacity = 16U;

  constexpr TelemetryFramer() noexcept = default;

  void reset() noexcept;

  TelemetryFrame pack_and_enqueue(const MgukActuatorOutput& mguk,
                                  const AeroActuatorOutput& aero) noexcept;

  bool pop_frame(TelemetryFrame& out) noexcept { return queue_.pop(out); }
  std::size_t queued_frames() const noexcept { return queue_.size(); }
  uint32_t overflow_count() const noexcept { return overflow_count_; }

  static uint16_t compute_crc16_ccitt(const uint8_t* data, std::size_t length) noexcept;

 private:
  RingBuffer<TelemetryFrame, kQueueCapacity> queue_{};
  uint16_t sequence_counter_{0U};
  uint32_t overflow_count_{0U};
};

}  // namespace secu
