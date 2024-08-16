#pragma once
#include <memory>
#include <vector>

#include "BufferCollection.hpp"
#include "common.hpp"
#include "Descriptor/DescriptorBuffer.hpp"
#include "Resources/AllocatedBuffer.hpp"
#include "VulkanTypes.hpp"
#include "Resources/TextureHandle.hpp"

namespace gestalt::foundation {

  struct MaterialBuffers final : BufferCollection, NonCopyable<MaterialBuffers> {
    std::shared_ptr<AllocatedBuffer> uniform_buffer;

    TextureHandle environment_map;
    TextureHandle environment_irradiance_map;
    TextureHandle bdrf_lut;

    VkSampler cube_map_sampler;

    std::shared_ptr<DescriptorBuffer> descriptor_buffer;

    std::vector<std::shared_ptr<AllocatedBuffer>> get_buffers(int16 frame_index) const override {
      return {uniform_buffer};
    }

    std::shared_ptr<DescriptorBuffer> get_descriptor_buffer(
        int16 frame_index) const override {
      return descriptor_buffer;
    }
  };
}  // namespace gestalt