#pragma once

#include <memory>
#include <unordered_map>

#include "common.hpp"
#include "Resources/ResourceTypes.hpp"

#include "Utils/Pipeline.hpp"

namespace gestalt::graphics {
  class ResourceAllocator;
}

namespace gestalt::graphics {
  struct FrameGraphNode;

  class ResourceRegistry {
    std::unordered_map<uint64, std::shared_ptr<ResourceInstance>> resource_map_;
    ResourceAllocator* resource_factory_ = nullptr;

  public:
    explicit ResourceRegistry(ResourceAllocator* resource_factory)
        : resource_factory_(resource_factory) {}

    std::shared_ptr<ImageInstance> add_template(ImageTemplate&& image_template);

    std::shared_ptr<BufferInstance> add_template(BufferTemplate&& buffer_template);


    template <typename ResourceInstanceType> std::shared_ptr<ResourceInstanceType> add_resource(
        std::shared_ptr<ResourceInstanceType> resource_instance) {
      if (resource_instance == nullptr) {
        throw std::runtime_error("Resource instance cannot be null!");
      }
      resource_instance->set_handle(reinterpret_cast<uint64>(resource_instance.get()));

      resource_map_.insert({resource_instance->handle(), resource_instance});

      auto it = resource_map_.find(resource_instance->handle());
      if (it == resource_map_.end()) {
        throw std::runtime_error("Failed to insert resource into registry!");
      }

      return std::static_pointer_cast<ResourceInstanceType>(it->second);
    }

    std::shared_ptr<ResourceInstance> get_resource(const uint32 handle);
  };

}  // namespace gestalt::graphics::fg