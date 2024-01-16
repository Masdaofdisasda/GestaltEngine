#pragma once

#include "vk_gpu.h"

#include <vk_types.h>

#include "vk_deletion_service.h"
#include "vk_descriptors.h"

struct frame_data {
  VkSemaphore swapchain_semaphore, render_semaphore;
  VkFence render_fence;

  VkCommandPool command_pool;
  VkCommandBuffer main_command_buffer;

  vk_deletion_service deletion_queue;
  DescriptorAllocatorGrowable frame_descriptors;
};

class vk_command {

  vk_gpu gpu_;
  vk_deletion_service deletion_service_;

public:
  VkCommandBuffer imgui_command_buffer;
  VkCommandPool imgui_command_pool;

  void init(const vk_gpu& gpu, vk_deletion_service& deletion_service, std::vector<frame_data>& frames);
};
