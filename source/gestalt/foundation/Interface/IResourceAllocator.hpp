#pragma once
#include "Resources/ResourceTypes.hpp"

namespace gestalt::foundation {
  class IGpu;

  class IResourceAllocator {
  public:
    virtual ~IResourceAllocator() = default;

    virtual std::shared_ptr<ImageInstance> create_image(ImageTemplate&& image_template) = 0;

    virtual std::shared_ptr<BufferInstance> create_buffer(BufferTemplate&& buffer_template) const
        = 0;
    virtual void destroy_buffer(const std::shared_ptr<BufferInstance>& buffer) const = 0;
  };
}  // namespace gestalt