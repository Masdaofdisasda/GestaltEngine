﻿#pragma once

#include "vk_gpu.h"

#include <vk_types.h>

#include "vk_deletion_service.h"
#include "vk_descriptors.h"

class vk_command {

  vk_gpu gpu_;
  vk_deletion_service deletion_service_;

public:
  VkCommandBuffer imgui_command_buffer;
  VkCommandPool imgui_command_pool;

  void init(const vk_gpu& gpu, std::vector<frame_data>& frames);
  void cleanup();
};