#pragma once

#include "common.hpp"
#include <glm/gtx/transform.hpp>

namespace gestalt::foundation {

  struct alignas(32) GpuPointLight {
    glm::vec3 color;
    float32 intensity;
    glm::vec3 position;
    float32 range;
  };

}  // namespace gestalt