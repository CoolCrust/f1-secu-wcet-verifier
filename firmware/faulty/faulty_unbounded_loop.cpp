#include <cstdint>

namespace secu::faulty {

// VIOLATION 1 (WCET / ISO 26262): Unbounded hardware polling loop with no max iteration cap.
// If the CAN transceiver status register sticks due to EMI, this 1 kHz ISR deadlocks.
uint32_t wait_for_can_mailbox_ready(volatile const uint32_t* status_reg) noexcept {
  uint32_t spins = 0U;
  while ((*status_reg & 0x01U) == 0U) {
    ++spins;
  }
  return spins;
}

}  // namespace secu::faulty
