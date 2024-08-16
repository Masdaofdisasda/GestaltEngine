#pragma once

namespace gestalt::foundation {

    struct alignas(16) PerFrameData {
      glm::mat4 view{1.f};
      glm::mat4 inv_view{1.f};
      glm::mat4 proj{1.f};
      glm::mat4 inv_viewProj{1.f};
      glm::mat4 cullView{1.f};
      glm::mat4 cullProj{1.f};
      float P00, P11, znear, zfar;  // symmetric projection parameters
      glm::vec4 frustum[6];
    };
}  // namespace gestalt