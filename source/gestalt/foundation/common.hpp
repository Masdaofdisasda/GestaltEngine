#pragma once

#include <cstdint>

using uint64 = uint64_t;
using int64 = int64_t;
using uint32 = uint32_t;
using int32 = int32_t;
using uint16 = uint16_t;
using int16 = int16_t;
using uint8 = uint8_t;
using int8 = int8_t;

using float32 = float;
using float64 = double;

// Constants
constexpr float32 kPi = 3.14159265358979323846f;
constexpr float64 kPiDouble = 3.14159265358979323846;

namespace gestalt {

  namespace foundation {
    enum class TextureType;
    class TextureHandle;
  }  // namespace foundation

  namespace application {
    using namespace foundation;
  }  // namespace application

  namespace graphics {
    using namespace application;
    using namespace foundation;
    namespace vkutil {
      
    }
  }  // namespace graphics

  class GameEngine;

}  // namespace gestalt