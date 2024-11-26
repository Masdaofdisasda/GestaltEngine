#include "ResourceFactory.hpp"

#include <cassert>

#include "Render Engine/FrameGraphTypes.hpp"

namespace gestalt::graphics {
  void ResourceFactory::set_debug_name(const std::string& name, const VkObjectType type,
      const uint64 handle) const {
    assert(!name.empty());

    VkDebugUtilsObjectNameInfoEXT name_info = {};
    name_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    name_info.objectType = type;
    name_info.objectHandle = handle;
    name_info.pObjectName = name.c_str();
    vkSetDebugUtilsObjectNameEXT(gpu_->getDevice(), &name_info);
  }

  std::shared_ptr<fg::ImageResource> ResourceFactory::create_image(fg::ImageResourceTemplate& image_template) {
    return std::make_shared<fg::ImageResource>(std::move(image_template));
  }
}  // namespace gestalt