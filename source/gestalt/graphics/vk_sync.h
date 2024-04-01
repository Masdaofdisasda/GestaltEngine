#pragma once

#include "vk_commands.h"
#include "Gpu.h"
#include "vk_types.h"

class vk_sync {
  Gpu gpu_;
  DeletionService deletion_service_;

public:
  VkFence imgui_fence;

  void init(const Gpu& gpu, std::vector<FrameData>& frames);
  void cleanup() { deletion_service_.flush(); }
};
