#pragma once
#include <memory>
#include <vector>

#include "BufferCollection.hpp"
#include "common.hpp"
#include "Resources/ResourceTypes.hpp"

namespace gestalt::foundation {

  struct LightBuffers final : BufferCollection {

    std::shared_ptr<BufferInstance> dir_light_buffer_instance;
  std::shared_ptr<BufferInstance> point_light_buffer_instance;
    std::shared_ptr<BufferInstance> view_proj_matrices_instance;

  std::vector<std::shared_ptr<BufferInstance>> get_buffers(int16 frame_index) const override {
    return {dir_light_buffer_instance, point_light_buffer_instance, view_proj_matrices_instance};
  }

};
}  // namespace gestalt