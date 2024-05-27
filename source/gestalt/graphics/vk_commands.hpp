#pragma once

#include "Gpu.hpp"

#include <vk_types.hpp>

#include "DeletionService.hpp"
#include "vk_descriptors.hpp"

namespace gestalt {

  class VkCommand {
    graphics::Gpu gpu_;
    DeletionService deletion_service_;

  public:
    VkCommandBuffer imgui_command_buffer;
    VkCommandPool imgui_command_pool;

    void init(const graphics::Gpu& gpu, std::vector<graphics::FrameData>& frames);
    void cleanup();
  };

}  // namespace gestalt