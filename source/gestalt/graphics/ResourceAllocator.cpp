#include "ResourceAllocator.hpp"

#include <stb_image.h>

#include <cassert>

#include "EngineConfiguration.hpp"
#include "VulkanCheck.hpp"
#include "vk_initializers.hpp"

namespace gestalt::graphics {

  VkImageUsageFlags get_usage_flags(const TextureType type) {
    VkImageUsageFlags usage_flags = 0;
    if (type == TextureType::kColor) {
      usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
      usage_flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
      usage_flags |= VK_IMAGE_USAGE_STORAGE_BIT;
      usage_flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
      usage_flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    } else if (type == TextureType::kDepth) {
      usage_flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      usage_flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
      usage_flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }
    return usage_flags;
  }

  VkImageAspectFlags get_aspect_flags(const TextureType type) {
    if (type == TextureType::kColor) {
      return VK_IMAGE_ASPECT_COLOR_BIT;
    }
    if (type == TextureType::kDepth) {
      return VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    return 0;
  }

  std::shared_ptr<ImageInstance> ResourceAllocator::create_image(ImageTemplate&& image_template) {
    assert(image_template.format != VK_FORMAT_UNDEFINED && "Image format must be specified.");

    const VkImageUsageFlags usage_flags = get_usage_flags(image_template.type);

    const VkImageAspectFlags aspect_flags = get_aspect_flags(image_template.type);


    if (std::holds_alternative<std::filesystem::path>(image_template.initial_value)) {
      const auto path = std::get<std::filesystem::path>(image_template.initial_value);
      const ImageInfo image_info(path);

      auto allocated_image = allocate_image(image_template.name, image_info.get_format(), usage_flags,
                                            image_info.get_extent(), aspect_flags);

      task_queue_.enqueue([this, path](VkCommandBuffer cmd) {
        ImageData image_data(path);
        // todo : copy image data to image
      });

      return std::make_unique<ImageInstance>(std::move(image_template), allocated_image,
                                                 image_info.get_extent());
    }

    const auto image_size = image_template.image_size;
    VkExtent3D extent = {};
    if (std::holds_alternative<RelativeImageSize>(image_size)) {
      const auto scale = std::get<RelativeImageSize>(image_size).scale;
      extent = {static_cast<uint32>(getWindowedWidth() * scale),
                static_cast<uint32>(getWindowedHeight() * scale), 1};
    } else if (std::holds_alternative<AbsoluteImageSize>(image_size)) {
      extent  = std::get<AbsoluteImageSize>(image_size).extent;
    } else {
      assert(false && "Invalid image size type.");
    }
    if (image_template.image_type == ImageType::kImage2D) {
      extent.depth = 1;
    }

    auto allocated_image
        = allocate_image(image_template.name, image_template.format, usage_flags, extent, aspect_flags);

    return std::make_unique<ImageInstance>(std::move(image_template), allocated_image, extent);
  }

  ImageInfo::ImageInfo(const std::filesystem::path& path) {
    stbi_info(path.string().c_str(), &width, &height, &channels);
  }

  ImageData::ImageData(const std::filesystem::path& path)
    : image_info(path) {
    data = stbi_load(path.string().c_str(), &image_info.width, &image_info.height,
                      &image_info.channels,
                      STBI_rgb_alpha);
    if (!data) {
      throw std::runtime_error("Failed to load image data from file {" + path.string() + "}.");
    }
  }

  ImageData::~ImageData() {
    if (data) {
      stbi_image_free(data);
    }
  }

  AllocatedImage ResourceAllocator::allocate_image(const std::string_view name, const VkFormat format,
                                                     const VkImageUsageFlags usage_flags, const VkExtent3D extent, const VkImageAspectFlags aspect_flags) const {
    const VkImageCreateInfo img_info = vkinit::image_create_info(format, usage_flags, extent);

    const VmaAllocationCreateInfo allocation_info = {
      .usage = VMA_MEMORY_USAGE_GPU_ONLY,
      .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    };

    AllocatedImage image;
    VK_CHECK(vmaCreateImage(gpu_->getAllocator(), &img_info, &allocation_info, &image.image_handle,
                            &image.allocation, nullptr));
    gpu_->set_debug_name(name, VK_OBJECT_TYPE_IMAGE,
                   reinterpret_cast<uint64_t>(image.image_handle));

    const VkImageViewCreateInfo view_info
        = vkinit::imageview_create_info(format, image.image_handle, aspect_flags);
    VK_CHECK(vkCreateImageView(gpu_->getDevice(), &view_info, nullptr, &image.image_view));

    return image;
  }

  AllocatedBuffer ResourceAllocator::allocate_buffer(const std::string_view name, const VkDeviceSize size,
                                                       VkBufferUsageFlags usage_flags,
                                                       const VmaMemoryUsage memory_usage) const {
    usage_flags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    const VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage_flags,
    };

    const VmaAllocationCreateInfo allocation_info = {
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = memory_usage,
    };

    AllocatedBuffer buffer;
    VK_CHECK(vmaCreateBuffer(gpu_->getAllocator(), &buffer_info, &allocation_info,
                             &buffer.buffer_handle, &buffer.allocation, &buffer.info));
    gpu_->set_debug_name(name, VK_OBJECT_TYPE_BUFFER,
                         reinterpret_cast<uint64_t>(buffer.buffer_handle));

    const VkBufferDeviceAddressInfo device_address_info
        = {.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = buffer.buffer_handle};
    buffer.address = vkGetBufferDeviceAddress(gpu_->getDevice(), &device_address_info);

    return buffer;
  }

  std::shared_ptr<BufferInstance> ResourceAllocator::create_buffer(
      BufferTemplate&& buffer_template) const {

      auto allocated_buffer = allocate_buffer(buffer_template.name, buffer_template.size, buffer_template.usage,
                            buffer_template.memory_usage);

    return std::make_shared<BufferInstance>(std::move(buffer_template), allocated_buffer);
  }
}  // namespace gestalt