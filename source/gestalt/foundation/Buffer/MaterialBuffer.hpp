#pragma once
#include <memory>
#include <vector>

#include "BufferCollection.hpp"
#include "common.hpp"
#include "Descriptor/DescriptorBuffer.hpp"
#include "VulkanTypes.hpp"
#include "Resources/TextureHandleOld.hpp"

namespace gestalt::foundation {

  struct MaterialBuffers final : BufferCollection, NonCopyable<MaterialBuffers> {
    TextureHandleOld environment_map;
    TextureHandleOld environment_irradiance_map;
    TextureHandleOld bdrf_lut;

    VkSampler cube_map_sampler;

    std::shared_ptr<BufferInstance> material_buffer;

    std::shared_ptr<DescriptorBuffer> descriptor_buffer;

    std::vector<std::shared_ptr<BufferInstance>> get_buffers(int16 frame_index) const override {
      return {material_buffer};
    }

    std::shared_ptr<DescriptorBuffer> get_descriptor_buffer(
        int16 frame_index) const override {
      return descriptor_buffer;
    }
  };
}  // namespace gestalt