#pragma once
#include <memory>
#include <vector>

#include "BufferCollection.hpp"
#include "common.hpp"

namespace gestalt::foundation {

  struct MaterialBuffers final : BufferCollection, NonCopyable<MaterialBuffers> {

    std::shared_ptr<BufferInstance> material_buffer;

    std::vector<std::shared_ptr<BufferInstance>> get_buffers(int16 frame_index) const override {
      return {material_buffer};
    }
  };
}  // namespace gestalt