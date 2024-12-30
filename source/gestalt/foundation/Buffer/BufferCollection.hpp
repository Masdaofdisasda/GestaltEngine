#pragma once
#include <memory>
#include <vector>

#include "common.hpp"
#include "Resources/ResourceTypes.hpp"

namespace gestalt::foundation {

  struct BufferCollection {
    virtual ~BufferCollection() = default;
    virtual std::vector<std::shared_ptr<BufferInstance>> get_buffers(int16 frame_index) const = 0;
  };
}  // namespace gestalt