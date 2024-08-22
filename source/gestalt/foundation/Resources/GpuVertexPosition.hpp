#pragma once

#include "common.hpp"

namespace gestalt::foundation {

  struct alignas(4) GpuVertexPosition {
    glm::vec3 position{0.f};
    float32 padding{0.f};
  };


}  // namespace gestalt