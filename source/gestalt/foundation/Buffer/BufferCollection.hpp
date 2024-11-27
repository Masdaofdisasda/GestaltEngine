#pragma once
#include <memory>
#include <vector>

#include "common.hpp"
#include "Descriptor/DescriptorBuffer.hpp"
#include "Resources/AllocatedBufferOld.hpp"

namespace gestalt::foundation {

  struct BufferCollection {
    virtual ~BufferCollection() = default;
    virtual std::vector<std::shared_ptr<AllocatedBufferOld>> get_buffers(int16 frame_index) const = 0;
    virtual std::shared_ptr<DescriptorBuffer> get_descriptor_buffer(
        int16 frame_index) const
        = 0;
  };
}  // namespace gestalt