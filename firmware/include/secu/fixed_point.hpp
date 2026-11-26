#pragma once

#include <compare>
#include <cstdint>
#include <limits>

namespace secu {

// Deterministic Q16.16 signed fixed-point type for ARM Cortex-M4/M7 without FPU reliance.
// Range: [-32768.0, +32767.99998], Resolution: 2^-16 (~0.00001526).
class Q16_16 {
 public:
  constexpr Q16_16() noexcept : raw_(0) {}
  constexpr explicit Q16_16(int32_t raw_value) noexcept : raw_(raw_value) {}

  static constexpr Q16_16 zero() noexcept { return Q16_16{0}; }

  static constexpr Q16_16 from_int(int32_t value) noexcept {
    const int64_t shifted = static_cast<int64_t>(value) * 65536LL;
    return saturate_i32(shifted);
  }

  static constexpr Q16_16 from_milli(int32_t milli_value) noexcept {
    const int64_t scaled = (static_cast<int64_t>(milli_value) * 65536LL) / 1000LL;
    return saturate_i32(scaled);
  }

  static consteval Q16_16 from_literal(double value) noexcept {
    const double scaled = value * 65536.0;
    if (scaled >= static_cast<double>(std::numeric_limits<int32_t>::max())) {
      return Q16_16{std::numeric_limits<int32_t>::max()};
    }
    if (scaled <= static_cast<double>(std::numeric_limits<int32_t>::min())) {
      return Q16_16{std::numeric_limits<int32_t>::min()};
    }
    const double rounded = (scaled >= 0.0) ? (scaled + 0.5) : (scaled - 0.5);
    return Q16_16{static_cast<int32_t>(rounded)};
  }

  // Fused 64-bit (a * b) / c preventing intermediate Q16.16 saturation.
  static constexpr Q16_16 mul_div(Q16_16 a, Q16_16 b, Q16_16 c) noexcept {
    if (c.raw_ == 0) {
      const bool non_neg = (a.raw_ >= 0) == (b.raw_ >= 0);
      return non_neg ? Q16_16{std::numeric_limits<int32_t>::max()}
                     : Q16_16{std::numeric_limits<int32_t>::min()};
    }
    const int64_t wide_num =
        static_cast<int64_t>(a.raw_) * static_cast<int64_t>(b.raw_);
    return saturate_i32(wide_num / static_cast<int64_t>(c.raw_));
  }

  constexpr int32_t raw() const noexcept { return raw_; }
  constexpr int32_t to_int_trunc() const noexcept { return static_cast<int32_t>(raw_ / 65536); }
  constexpr int32_t to_milli() const noexcept {
    return static_cast<int32_t>((static_cast<int64_t>(raw_) * 1000LL) / 65536LL);
  }

  constexpr Q16_16 abs() const noexcept {
    if (raw_ == std::numeric_limits<int32_t>::min()) {
      return Q16_16{std::numeric_limits<int32_t>::max()};
    }
    return Q16_16{raw_ < 0 ? -raw_ : raw_};
  }

  constexpr Q16_16 operator-() const noexcept {
    if (raw_ == std::numeric_limits<int32_t>::min()) {
      return Q16_16{std::numeric_limits<int32_t>::max()};
    }
    return Q16_16{-raw_};
  }

  constexpr Q16_16 operator+(Q16_16 rhs) const noexcept {
    return saturate_i32(static_cast<int64_t>(raw_) + static_cast<int64_t>(rhs.raw_));
  }

  constexpr Q16_16 operator-(Q16_16 rhs) const noexcept {
    return saturate_i32(static_cast<int64_t>(raw_) - static_cast<int64_t>(rhs.raw_));
  }

  constexpr Q16_16 operator*(Q16_16 rhs) const noexcept {
    const int64_t product =
        (static_cast<int64_t>(raw_) * static_cast<int64_t>(rhs.raw_)) >> 16;
    return saturate_i32(product);
  }

  constexpr Q16_16 operator/(Q16_16 rhs) const noexcept {
    if (rhs.raw_ == 0) {
      return (raw_ >= 0) ? Q16_16{std::numeric_limits<int32_t>::max()}
                         : Q16_16{std::numeric_limits<int32_t>::min()};
    }
    const int64_t numerator = static_cast<int64_t>(raw_) * 65536LL;
    return saturate_i32(numerator / static_cast<int64_t>(rhs.raw_));
  }

  constexpr auto operator<=>(const Q16_16&) const noexcept = default;

  static constexpr Q16_16 clamp(Q16_16 val, Q16_16 lo, Q16_16 hi) noexcept {
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
  }

  static constexpr Q16_16 min(Q16_16 a, Q16_16 b) noexcept { return (a < b) ? a : b; }

 private:
  static constexpr Q16_16 saturate_i32(int64_t wide) noexcept {
    if (wide > static_cast<int64_t>(std::numeric_limits<int32_t>::max())) {
      return Q16_16{std::numeric_limits<int32_t>::max()};
    }
    if (wide < static_cast<int64_t>(std::numeric_limits<int32_t>::min())) {
      return Q16_16{std::numeric_limits<int32_t>::min()};
    }
    return Q16_16{static_cast<int32_t>(wide)};
  }

  int32_t raw_;
};

}  // namespace secu
