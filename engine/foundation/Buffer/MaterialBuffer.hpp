#pragma once
#include <memory>

namespace gestalt::foundation {

  struct MaterialBuffers final {
    MaterialBuffers() = default;
    ~MaterialBuffers() = default;

    MaterialBuffers(const MaterialBuffers&) = delete;
    MaterialBuffers& operator=(const MaterialBuffers&) = delete;

    MaterialBuffers(MaterialBuffers&&) = delete;
    MaterialBuffers& operator=(MaterialBuffers&&) = delete;

    std::shared_ptr<BufferInstance> material_buffer;
  };
}  // namespace gestalt