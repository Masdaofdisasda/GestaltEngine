#pragma once

#include "Resources/AccelerationStructure.hpp"
#include <memory>

#include "Resources/ResourceTypes.hpp"

namespace gestalt::foundation {

  struct RayTracingBuffer final {

    std::shared_ptr<BufferInstance> bottom_level_acceleration_structure_buffer;
    std::vector<std::shared_ptr<AccelerationStructure>> bottom_level_acceleration_structures;
  };
}  // namespace gestalt