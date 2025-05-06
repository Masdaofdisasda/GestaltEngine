#pragma once

#include "common.hpp"
#include <glm/gtx/transform.hpp>

namespace gestalt::foundation {

    struct alignas(16) PerFrameData {
      glm::mat4 view{1.f};
      glm::mat4 inv_view{1.f};
      glm::mat4 proj{1.f};
      glm::mat4 inv_viewProj{1.f};
      glm::mat4 cullView{1.f};
      glm::mat4 cullProj{1.f};
      float32 P00{0.f}, P11{0.f}, znear{0.1f}, zfar{1000.f};  // symmetric projection parameters
      glm::vec4 frustum[6] = {};
    };
}  // namespace gestalt