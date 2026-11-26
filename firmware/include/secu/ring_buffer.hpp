#pragma once

#include <array>
#include <cstddef>

namespace secu {

// Zero-allocation static power-of-two ring buffer with O(1) bounded execution.
template <typename T, std::size_t Capacity>
class RingBuffer {
  static_assert(Capacity > 0U, "RingBuffer capacity must be non-zero");
  static_assert((Capacity & (Capacity - 1U)) == 0U, "RingBuffer capacity must be a power of two");

 public:
  constexpr RingBuffer() noexcept = default;

  constexpr bool empty() const noexcept { return count_ == 0U; }
  constexpr bool full() const noexcept { return count_ == Capacity; }
  constexpr std::size_t size() const noexcept { return count_; }
  constexpr std::size_t capacity() const noexcept { return Capacity; }

  constexpr void clear() noexcept {
    head_ = 0U;
    tail_ = 0U;
    count_ = 0U;
  }

  // Deterministic non-blocking push: returns false if buffer is full.
  constexpr bool push(const T& item) noexcept {
    if (full()) {
      return false;
    }
    storage_[head_] = item;
    head_ = (head_ + 1U) & kMask;
    ++count_;
    return true;
  }

  // Deterministic telemetry push: overwrites oldest entry when full.
  constexpr void push_overwrite(const T& item) noexcept {
    if (full()) {
      tail_ = (tail_ + 1U) & kMask;
      --count_;
    }
    storage_[head_] = item;
    head_ = (head_ + 1U) & kMask;
    ++count_;
  }

  constexpr bool pop(T& out) noexcept {
    if (empty()) {
      return false;
    }
    out = storage_[tail_];
    tail_ = (tail_ + 1U) & kMask;
    --count_;
    return true;
  }

 private:
  static constexpr std::size_t kMask = Capacity - 1U;
  std::array<T, Capacity> storage_{};
  std::size_t head_{0U};
  std::size_t tail_{0U};
  std::size_t count_{0U};
};

}  // namespace secu
