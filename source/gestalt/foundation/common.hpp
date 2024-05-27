#pragma once

using uint64 = std::uint64_t;
using int64 = std::int64_t;
using uint32 = std::uint32_t;
using int32 = std::int32_t;
using uint16 = std::uint16_t;
using int16 = std::int16_t;
using uint8 = std::uint8_t;
using int8 = std::int8_t;

using float32 = float;
using float64 = double;

// Constants
constexpr float32 PI = 3.14159265358979323846f;
constexpr float64 PI_DOUBLE = 3.14159265358979323846;

namespace gestalt {

  namespace graphics {
    using namespace application;
    using namespace foundation;
  }  // namespace graphics

  namespace application {
    using namespace foundation;
  }  // namespace application

  namespace foundation {
    // Declarations related to the foundation
  }
}  // namespace gestalt