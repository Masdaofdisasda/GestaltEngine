#pragma once

#include <memory>

#include "common.hpp"
#include "VulkanTypes.hpp"
#include "Interface/IGpu.hpp"

namespace gestalt::graphics::fg {
  struct ImageResource;
  struct ImageResourceTemplate;
}

namespace gestalt::graphics {

  class ResourceFactory final : public NonCopyable<ResourceFactory> {

      IGpu* gpu_ = nullptr;

    public:
      explicit ResourceFactory(IGpu* gpu) : gpu_(gpu) {}

      void set_debug_name(const std::string& name, VkObjectType type,
                          uint64 handle) const;
      std::shared_ptr<fg::ImageResource> create_image(fg::ImageResourceTemplate& image_template);
    };
}  // namespace gestalt