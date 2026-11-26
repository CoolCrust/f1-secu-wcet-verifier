#include "secu/telemetry_framer.hpp"

namespace secu {

namespace {

constexpr uint8_t kSyncByte0 = 0xF1U;
constexpr uint8_t kSyncByte1 = 0x26U;

constexpr void write_u16_be(std::array<uint8_t, kTelemetryFrameBytes>& buf,
                            std::size_t offset,
                            uint16_t val) noexcept {
  buf[offset] = static_cast<uint8_t>((val >> 8U) & 0xFFU);
  buf[offset + 1U] = static_cast<uint8_t>(val & 0xFFU);
}

constexpr void write_i32_be(std::array<uint8_t, kTelemetryFrameBytes>& buf,
                            std::size_t offset,
                            int32_t val) noexcept {
  const uint32_t uval = static_cast<uint32_t>(val);
  buf[offset] = static_cast<uint8_t>((uval >> 24U) & 0xFFU);
  buf[offset + 1U] = static_cast<uint8_t>((uval >> 16U) & 0xFFU);
  buf[offset + 2U] = static_cast<uint8_t>((uval >> 8U) & 0xFFU);
  buf[offset + 3U] = static_cast<uint8_t>(uval & 0xFFU);
}

}  // namespace

void TelemetryFramer::reset() noexcept {
  queue_.clear();
  sequence_counter_ = 0U;
  overflow_count_ = 0U;
}

uint16_t TelemetryFramer::compute_crc16_ccitt(const uint8_t* data,
                                              std::size_t length) noexcept {
  uint16_t crc = 0xFFFFU;
  for (std::size_t i = 0U; i < length; ++i) {
    crc = static_cast<uint16_t>(
        static_cast<uint32_t>(crc) ^ (static_cast<uint32_t>(data[i]) << 8U));
    for (uint8_t bit = 0U; bit < 8U; ++bit) {
      if ((crc & 0x8000U) != 0U) {
        crc = static_cast<uint16_t>((static_cast<uint32_t>(crc) << 1U) ^ 0x1021U);
      } else {
        crc = static_cast<uint16_t>(static_cast<uint32_t>(crc) << 1U);
      }
    }
  }
  return crc;
}

TelemetryFrame TelemetryFramer::pack_and_enqueue(const MgukActuatorOutput& mguk,
                                                 const AeroActuatorOutput& aero) noexcept {
  TelemetryFrame frame{};
  frame.bytes[0] = kSyncByte0;
  frame.bytes[1] = kSyncByte1;

  write_u16_be(frame.bytes, 2U, sequence_counter_++);
  frame.bytes[4] = static_cast<uint8_t>(aero.active_mode);
  frame.bytes[5] = static_cast<uint8_t>(aero.executed_substeps & 0xFFU);

  const uint16_t combined_faults =
      static_cast<uint16_t>(mguk.fault_bits | aero.fault_bits);
  write_u16_be(frame.bytes, 6U, combined_faults);

  // Pack fixed-point Q16.16 telemetry fields (bytes 8..23)
  write_i32_be(frame.bytes, 8U, mguk.mguk_regen_power_kw.raw());
  write_i32_be(frame.bytes, 12U, mguk.hydraulic_rear_torque_nm.raw());
  write_i32_be(frame.bytes, 16U, mguk.lap_harvested_kj.raw());
  write_u16_be(frame.bytes, 20U, static_cast<uint16_t>(aero.front_flap_cmd_deg.to_milli()));
  write_u16_be(frame.bytes, 22U, static_cast<uint16_t>(aero.rear_flap_cmd_deg.to_milli()));

  // Compute CRC-16-CCITT over the 24-byte payload (SCEV-provable outer trip count = 24)
  const uint16_t crc = compute_crc16_ccitt(frame.bytes.data(), kTelemetryPayloadBytes);
  write_u16_be(frame.bytes, 24U, crc);

  if (!queue_.push(frame)) {
    ++overflow_count_;
    queue_.push_overwrite(frame);
  }

  return frame;
}

}  // namespace secu
