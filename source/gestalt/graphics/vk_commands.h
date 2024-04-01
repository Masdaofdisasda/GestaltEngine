#pragma once

#include "Gpu.h"

#include <vk_types.h>

#include "DeletionService.h"
#include "vk_descriptors.h"

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