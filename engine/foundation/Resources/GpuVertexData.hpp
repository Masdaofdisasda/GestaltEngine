#pragma once

#include "common.hpp"

namespace gestalt::foundation {

  struct alignas(4) GpuVertexData {
    uint8 normal[4];
    uint8 tangent[4];
    uint16 uv[2];
    float32 padding{0.f};
  };


}  // namespace gestalt