#pragma once

#include "VulkanCore.hpp"

namespace gestalt::foundation {
  struct AccelerationStructure {
    VkAccelerationStructureKHR acceleration_structure {};
    VkDeviceAddress            address;
  };
} // namespace gestalt