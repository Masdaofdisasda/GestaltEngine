#pragma once

#include "VulkanTypes.hpp"

namespace gestalt::foundation {
  struct AccelerationStructure {
    VkAccelerationStructureKHR acceleration_structure {};
    VkDeviceAddress            address;
  };
} // namespace gestalt