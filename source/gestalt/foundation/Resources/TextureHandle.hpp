#pragma once

#include "TextureType.hpp"

#include <vma/vk_mem_alloc.h>

namespace gestalt::foundation {

  class TextureHandle {
    VkFormat imageFormat = VK_FORMAT_UNDEFINED;
    TextureType type;
    VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  public:
    VkImage image = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkExtent3D imageExtent = {};

    TextureHandle(const TextureType type = TextureType::kColor) : type(type) {}

    VkExtent2D getExtent2D() const { return {imageExtent.width, imageExtent.height}; }
    void setLayout(VkImageLayout layout) { currentLayout = layout; }
    VkImageLayout getLayout() const { return currentLayout; }
    void setFormat(VkFormat format) { imageFormat = format; }
    VkFormat getFormat() const { return imageFormat; }
    TextureType getType() const { return type; }
  };


}  // namespace gestalt