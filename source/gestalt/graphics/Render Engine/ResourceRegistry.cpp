#include "ResourceRegistry.hpp"
#include "ResourceAllocator.hpp"

namespace gestalt::graphics {
  std::shared_ptr<ImageInstance> ResourceRegistry::add_template(ImageTemplate&& image_template) {
    const auto image = resource_factory_->create_image(std::move(image_template));

    return add_resource(image);
  }

  std::shared_ptr<BufferInstance> ResourceRegistry::add_template(BufferTemplate&& buffer_template) {
    const auto buffer = resource_factory_->create_buffer(std::move(buffer_template));

    return add_resource(buffer);
  }

  std::shared_ptr<ResourceInstance> ResourceRegistry::get_resource(const uint32 handle) {
    return resource_map_.at(handle);
  }
}  // namespace gestalt::graphics::fg