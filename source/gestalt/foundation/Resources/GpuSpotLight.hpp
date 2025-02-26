#pragma once

#include "common.hpp"
#include "glm/vec3.hpp"

namespace gestalt::foundation {

  struct alignas(32) GpuSpotLight {
    glm::vec3 color;
    float32 intensity;
    glm::vec3 position;
    float32 range;
    glm::vec3 direction;
    float32 inner_cone_angle;
    float32 outer_cone_angle;
    float32 pad;
  };

}  // namespace gestalt