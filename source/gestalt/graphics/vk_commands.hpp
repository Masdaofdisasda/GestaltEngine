#pragma once

#include "Gpu.hpp"

#include <vk_types.hpp>

#include "vk_descriptors.hpp"

namespace gestalt {

  class VkCommand {
    std::shared_ptr<foundation::IGpu> gpu_;

  public:
    VkCommandBuffer imgui_command_buffer;
    VkCommandPool imgui_command_pool;

    void init(const std::shared_ptr<foundation::IGpu>& gpu, std::vector<graphics::FrameData>& frames);
    void cleanup();
  };

}  // namespace gestalt