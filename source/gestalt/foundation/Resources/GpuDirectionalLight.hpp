#pragma once

#include "common.hpp"
#include <glm/gtx/transform.hpp>

namespace gestalt::foundation {

  struct alignas(32) GpuDirectionalLight {
    glm::vec3 color;
    float32 intensity;
    glm::vec3 direction;
    uint32 viewProj;
  };

}  // namespace gestalt