#pragma once

#include "vk_commands.hpp"
#include "Gpu.hpp"
#include "vk_types.hpp"

namespace gestalt::graphics {

    class vk_sync {
    std::shared_ptr<IGpu> gpu_;

  public:
    VkFence imgui_fence;

    void init(const std::shared_ptr<IGpu>& gpu, std::vector<FrameData>& frames);
    void cleanup() { }
  };
}  // namespace gestalt