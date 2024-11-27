#pragma once

#include "VulkanTypes.hpp"
#include <vk_mem_alloc.h>

namespace gestalt::foundation {

  struct AllocatedBufferOld {
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
    VkDeviceAddress address;

    VkAccessFlags2 currentAccess = 0;        // Current access flags
    VkPipelineStageFlags2 currentStage = 0;  // Current pipeline stage
    VkBufferUsageFlags usage;                // Buffer usage flags
    VmaMemoryUsage memory_usage;
  };


}  // namespace gestalt