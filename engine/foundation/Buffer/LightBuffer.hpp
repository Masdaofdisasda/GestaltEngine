#pragma once
#include <memory>

#include "Resources/ResourceTypes.hpp"

namespace gestalt::foundation {

  struct LightBuffers final {
    std::shared_ptr<BufferInstance> dir_light_buffer;
    std::shared_ptr<BufferInstance> point_light_buffer;
    std::shared_ptr<BufferInstance> spot_light_buffer;
    std::shared_ptr<BufferInstance> view_proj_matrices;
  };
}  // namespace gestalt::foundation