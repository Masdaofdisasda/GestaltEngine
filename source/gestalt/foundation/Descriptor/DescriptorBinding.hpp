#pragma once
#include "common.hpp"
#include "VulkanTypes.hpp"

namespace gestalt::foundation {

  struct DescriptorBinding {
    size_t descriptorSize; // size of a single descriptor in bytes
    uint32 descriptorCount{1}; // NOTE: descriptorCount is only relevant for image arrays
    uint32 binding; // binding index in the descriptor set
    VkDeviceSize offset; // offset into the descriptor buffer where this binding starts
  };
}  // namespace gestalt