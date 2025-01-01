#pragma once
#include <memory>
#include "common.hpp"

namespace gestalt::foundation {

  struct MaterialBuffers final : NonCopyable<MaterialBuffers> {

    std::shared_ptr<BufferInstance> material_buffer;
  };
}  // namespace gestalt