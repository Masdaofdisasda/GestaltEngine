#pragma once
#include <array>
#include <memory>
#include <vector>

#include "BufferCollection.hpp"
#include "EngineConfiguration.hpp"
#include "PerFrameData.hpp"
#include "common.hpp"
#include "Descriptor/DescriptorBuffer.hpp"

namespace gestalt::foundation {

  struct PerFrameDataBuffers final : BufferCollection {
    std::array<PerFrameData, getFramesInFlight()> data;
    bool freezeCullCamera = false;
    std::array<std::shared_ptr<BufferInstance>, getFramesInFlight()> uniform_buffers;

    std::shared_ptr<BufferInstance> uniform_buffers_instance;

    std::array<std::shared_ptr<DescriptorBuffer>, getFramesInFlight()> descriptor_buffers;

    std::vector<std::shared_ptr<BufferInstance>> get_buffers(int16 frame_index) const override {
      return {uniform_buffers[frame_index]};
    }

    std::shared_ptr<DescriptorBuffer> get_descriptor_buffer(
        int16 frame_index) const override {
      return descriptor_buffers[frame_index];
    }
  };
}  // namespace gestalt