#pragma once

#include "Gpu.h"

#include <vk_types.h>

#include "DeletionService.h"
#include "vk_descriptors.h"

class VkCommand {

  Gpu gpu_;
  DeletionService deletion_service_;

public:
  VkCommandBuffer imgui_command_buffer;
  VkCommandPool imgui_command_pool;

  void init(const Gpu& gpu, std::vector<FrameData>& frames);
  void cleanup();
};
