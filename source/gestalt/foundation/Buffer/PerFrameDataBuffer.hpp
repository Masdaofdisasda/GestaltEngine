#pragma once
#include <array>
#include <memory>

#include "EngineConfiguration.hpp"
#include "PerFrameData.hpp"

namespace gestalt::foundation {

  struct PerFrameDataBuffers final {
    std::array<PerFrameData, getFramesInFlight()> data;
    bool freezeCullCamera = false;

    std::shared_ptr<BufferInstance> camera_buffer;
  };
}  // namespace gestalt