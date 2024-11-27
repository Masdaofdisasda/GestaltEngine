#pragma once
#include <memory>
#include <vector>

#include "BufferCollection.hpp"
#include "common.hpp"
#include "Descriptor/DescriptorBuffer.hpp"
#include "Resources/AllocatedBufferOld.hpp"
#include "Resources/ResourceTypes.hpp"

namespace gestalt::foundation {

  struct LightBuffers final : BufferCollection {
  std::shared_ptr<AllocatedBufferOld> dir_light_buffer;
  std::shared_ptr<AllocatedBufferOld> point_light_buffer;
  std::shared_ptr<AllocatedBufferOld> view_proj_matrices;

    std::shared_ptr<BufferInstance> light_buffer;

  std::shared_ptr<DescriptorBuffer> descriptor_buffer;

  std::vector<std::shared_ptr<AllocatedBufferOld>> get_buffers(int16 frame_index) const override {
    return {dir_light_buffer, point_light_buffer, view_proj_matrices};
  }

  std::shared_ptr<DescriptorBuffer> get_descriptor_buffer(
      int16 frame_index) const override {
    return descriptor_buffer;
  }
};
}  // namespace gestalt