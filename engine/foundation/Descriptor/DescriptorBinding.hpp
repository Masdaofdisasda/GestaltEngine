#pragma once
#include "common.hpp"
#include "VulkanCore.hpp"

namespace gestalt::foundation {

  struct DescriptorBinding {
    size_t descriptor_size; // size of a single descriptor in bytes
    int32 descriptor_count{1}; // NOTE: descriptorCount is only relevant for image arrays
    uint32 binding; // binding index in the descriptor set
    VkDeviceSize offset; // offset into the descriptor buffer where this binding starts
  };
}  // namespace gestalt