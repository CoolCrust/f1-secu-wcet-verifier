#include <cstdint>

namespace secu::faulty {

// VIOLATION 3 (MISRA C++:2023):
// - Uninitialized local variable (Rule 11.6.1)
// - Implicit double -> int16_t narrowing conversion (Rule 8.0.1)
// - Unstructured goto jump (Rule 9.6.1)
// - Dynamic exception throw (Rule 18.1.1)
int16_t unsafe_flap_angle_quantizer(double sensor_voltage, bool sensor_ok) {
  int16_t uninitialized_bias;
  if (!sensor_ok) {
    goto fail_handler;
  }
  {
    // Implicit narrowing conversion from double to int16_t
    int16_t quantized = sensor_voltage * 100.0 + uninitialized_bias;
    return quantized;
  }

fail_handler:
  throw 42;
}

}  // namespace secu::faulty
