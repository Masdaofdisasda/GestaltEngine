#pragma once
#include "common.hpp"
#include <glm/gtx/transform.hpp>

namespace gestalt::foundation {

    struct LightBase {
        glm::vec3 color;
        float32 intensity;
      };

}  // namespace gestalt