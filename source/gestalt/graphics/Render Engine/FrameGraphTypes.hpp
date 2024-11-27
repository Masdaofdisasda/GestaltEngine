#pragma once
#include <cassert>
#include <memory>
#include <variant>
#include <filesystem>

#include <fmt/core.h>
#include "common.hpp"
#include "VulkanTypes.hpp"
#include <Resources/TextureType.hpp>

namespace gestalt::graphics::fg {

  struct ResourceTemplate {
    std::string name;

    explicit ResourceTemplate(std::string name) : name(std::move(name)) {}

    virtual ~ResourceTemplate() = default;
  };

  enum class ImageType { kImage2D, kImage3D, kCubeMap };

  // scaled from window/screen resolution
  struct RelativeImageSize {
    float32 scale{1.0f};

    explicit RelativeImageSize(const float32 scale = 1.0f) : scale(scale) {
      assert(scale > 0.0f && "Scale must be positive.");
      assert(scale <= 16.0f && "Scale cannot be higher than 16.0.");
    }
  };

  // fixed dimensions
  struct AbsoluteImageSize {
    VkExtent3D extent{0, 0, 0};

    AbsoluteImageSize(const uint32 width, const uint32 height, const uint32 depth = 0) {
      extent = {width, height, depth};
      assert(width > 0 && height > 0 && "Width and height must be positive.");
    }
  };

  struct ImageResourceTemplate final : ResourceTemplate {
    ImageType image_type = ImageType::kImage2D;
    TextureType type = TextureType::kColor;
    std::variant<VkClearValue, std::filesystem::path> initial_value
        = VkClearValue({.color = {0.f, 0.f, 0.f, 1.f}});
    std::variant<RelativeImageSize, AbsoluteImageSize> image_size = RelativeImageSize(1.f);
    VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;

    // Constructor with name and optional customization
    explicit ImageResourceTemplate(std::string name) : ResourceTemplate(std::move(name)) {}

    // Fluent setters for customization
    ImageResourceTemplate& set_image_type(const TextureType texture_type, const VkFormat format) {
      this->type = texture_type;
      this->format = format;
      return *this;
    }

    ImageResourceTemplate& set_initial_value(const VkClearColorValue& clear_value) {
      assert(type == TextureType::kColor && "Clear color only supported for color images.");
      this->initial_value = VkClearValue({.color = clear_value});
      return *this;
    }

    ImageResourceTemplate& set_initial_value(const VkClearDepthStencilValue& clear_value) {
      assert(type == TextureType::kDepth && "Clear depth only supported for depth images.");
      this->initial_value = VkClearValue({.depthStencil = clear_value});
      return *this;
    }

    ImageResourceTemplate& set_initial_value(const std::filesystem::path& path) {
      assert(type == TextureType::kColor && "path only supported for color images.");
      this->initial_value = path;
      return *this;
    }

    ImageResourceTemplate& set_image_size(const float32& relative_size) {
      this->image_size = RelativeImageSize(relative_size);
      return *this;
    }

    ImageResourceTemplate& set_image_size(const uint32 width, const uint32 height, const uint32 depth = 0) {
      this->image_size = AbsoluteImageSize(width, height, depth);
      return *this;
    }

    ImageResourceTemplate build() { return *this; }

  };

    struct BufferResourceTemplate final : ResourceTemplate {
    VkDeviceSize size;
    VkBufferUsageFlags usage;
    VmaMemoryUsage memory_usage;

    BufferResourceTemplate(std::string name, const VkDeviceSize size,
                           const VkBufferUsageFlags usage, const VmaMemoryUsage memory_usage)
        : ResourceTemplate(std::move(name)), size(size), usage(usage), memory_usage(memory_usage) {}
  };

  //TODO split into template and instance
  struct Resource {
    uint64 resource_handle = -1;
    ResourceTemplate resource_template;
    explicit Resource(ResourceTemplate&& resource_template) : resource_template(std::move(resource_template)) {}
    [[nodiscard]] std::string_view name() const { return resource_template.name; }
  };

  struct AllocatedImage {
    VkImage image_handle = VK_NULL_HANDLE;
    VkImageView image_view = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
  };

  struct ImageResource : Resource {
    ImageResource(ImageResourceTemplate&& image_template, const AllocatedImage& allocated_image,
                  const VkExtent3D extent)
        : Resource(std::move(image_template)), allocated_image(allocated_image), extent(extent) {}

    AllocatedImage allocated_image;
    VkExtent3D extent;
    VkImageLayout current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  };

  struct AllocatedBuffer {
    VkBuffer buffer_handle = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo info;
    VkDeviceAddress address;
  };

  struct BufferResource : Resource {
    BufferResource(BufferResourceTemplate&& buffer_template,
                   const AllocatedBuffer& allocated_buffer)
        : Resource(std::move(buffer_template)), allocated_buffer(allocated_buffer) {}

    AllocatedBuffer allocated_buffer; 
    VkAccessFlags2 current_access = 0;        // Current access flags
    VkPipelineStageFlags2 current_stage = 0;  // Current pipeline stage
  };


}  // namespace gestalt::graphics::fg