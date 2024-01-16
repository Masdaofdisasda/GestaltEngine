#pragma once

#include "vk_commands.h"
#include "vk_gpu.h"
#include "vk_types.h"

class vk_sync {
  vk_gpu gpu_;
  vk_deletion_service deletion_service_;

public:
  VkFence imgui_fence;

  void init(const vk_gpu& gpu, std::vector<frame_data>& frames);
  void cleanup() { deletion_service_.flush(); }
};
