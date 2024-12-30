#pragma once
#include <array>
#include <memory>
#include <vector>

#include "BufferCollection.hpp"
#include "EngineConfiguration.hpp"
#include "PerFrameData.hpp"
#include "common.hpp"

namespace gestalt::foundation {

  struct PerFrameDataBuffers final : BufferCollection {
    std::array<PerFrameData, getFramesInFlight()> data;
    bool freezeCullCamera = false;

    std::shared_ptr<BufferInstance> uniform_buffers_instance;

    std::vector<std::shared_ptr<BufferInstance>> get_buffers(int16 frame_index) const override {
      return {uniform_buffers_instance};
    }
  };
}  // namespace gestalt