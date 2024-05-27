#pragma once

#include "vk_commands.hpp"
#include "Gpu.hpp"
#include "vk_types.hpp"

namespace gestalt {
  namespace graphics {

    class vk_sync {
      Gpu gpu_;
      DeletionService deletion_service_;

    public:
      VkFence imgui_fence;

      void init(const graphics::Gpu& gpu, std::vector<FrameData>& frames);
      void cleanup() { deletion_service_.flush(); }
    };
  }  // namespace graphics
}  // namespace gestalt