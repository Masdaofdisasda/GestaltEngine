#pragma once

#include <glm/fwd.hpp>

namespace gestalt::foundation {

  struct alignas(16) GpuProjViewData {
    glm::mat4 view;
    glm::mat4 proj;
  };

}  // namespace gestalt