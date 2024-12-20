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

  std::shared_ptr<ImageInstance> ResourceAllocator::create_image(ImageTemplate&& image_template) {
    if (image_template.format == VK_FORMAT_UNDEFINED) {
      throw std::runtime_error("Image format must be specified.");
    }

    const VkImageUsageFlags usage_flags = get_usage_flags(image_template.type);

    const auto image_size = image_template.image_size;
    VkExtent3D extent = {};
    if (std::holds_alternative<RelativeImageSize>(image_size)) {
      const auto scale = std::get<RelativeImageSize>(image_size).scale;
      extent = {static_cast<uint32>(getWindowedWidth() * scale),
                static_cast<uint32>(getWindowedHeight() * scale), 1};
    } else if (std::holds_alternative<AbsoluteImageSize>(image_size)) {
      extent = std::get<AbsoluteImageSize>(image_size).extent;
    } else {
      throw std::runtime_error("Invalid image size type.");
    }
    if (image_template.image_type == ImageType::kImage2D) {
      extent.depth = 1;
    }


    if (std::holds_alternative<std::filesystem::path>(image_template.initial_value)) {
      const auto path = std::get<std::filesystem::path>(image_template.initial_value);
      const ImageInfo image_info(path);

      auto allocated_image = allocate_image(image_template.name, image_info.get_format(), usage_flags,
                                            image_info.get_extent(), image_template.aspect_flags);

      task_queue_.add_image(path, allocated_image.image_handle);

      return std::make_unique<ImageInstance>(std::move(image_template), allocated_image,
                                                 image_info.get_extent());
    }

    if (std::holds_alternative<std::vector<unsigned char>>(image_template.initial_value)) {
      auto data = std::get<std::vector<unsigned char>>(image_template.initial_value);
      const ImageInfo image_info(data.data(), data.size(), extent);

      auto allocated_image = allocate_image(image_template.name, image_info.get_format(),
                                            usage_flags,
                                            image_info.get_extent(), image_template.aspect_flags);

      task_queue_.add_image(data, allocated_image.image_handle, image_info.get_extent());

      return std::make_unique<ImageInstance>(std::move(image_template), allocated_image,
                                             image_info.get_extent());
    }

    auto allocated_image = allocate_image(image_template.name, image_template.format, usage_flags,
                                          extent, image_template.aspect_flags);

    return std::make_unique<ImageInstance>(std::move(image_template), allocated_image, extent);
  }

  ImageInfo::ImageInfo(const std::filesystem::path& path) {
    if (!stbi_info(path.string().c_str(), &width, &height, &channels)) {
      throw std::runtime_error("Failed to read image info from file: " + path.string());
    }
    isEncodedData = true;
  }

  ImageInfo::ImageInfo(const unsigned char* data, const size_t size, const VkExtent3D extent) {
    if (stbi_info_from_memory(data, static_cast<int>(size), &width, &height, &channels)) {
      isEncodedData = true;
      return;
    }
    
      fmt::print("Failed to read image metadata from memory. Using extent as fallback\n");
      width = extent.width;
      height = extent.height;
      channels = 4;
      isEncodedData = false;
  }

  ImageData::ImageData(const std::filesystem::path& path)
    : image_info(path) {
    unsigned char* decoded_data = stbi_load(path.string().c_str(), &image_info.width,
                                            &image_info.height,
                                            &image_info.channels,
                                            STBI_rgb_alpha);
    if (image_info.channels == 3) {
      // Assume RGBA because many GPUs don't support RGB
      image_info.channels = 4;
    }

    const size_t data_size = static_cast<size_t>(image_info.width) * image_info.height * 4;
    data.assign(decoded_data, decoded_data + data_size);

    stbi_image_free(decoded_data);

    if (!data.data()) {
      throw std::runtime_error("Failed to load image data from file {" + path.string() + "}.");
    }
  }

  ImageData::ImageData(std::vector<unsigned char> data, const VkExtent3D extent)
    : image_info(data.data(), data.size(), extent) {
    if (image_info.isEncodedData) {
      unsigned char* decoded_data = stbi_load_from_memory(data.data(),
                                                          static_cast<int>(data.size()),
                                                          &image_info.width,
                                                          &image_info.height, &image_info.channels,
                                                          STBI_rgb_alpha);

      const size_t data_size = static_cast<size_t>(image_info.width) * image_info.height * 4;
      data.assign(decoded_data, decoded_data + data_size);

      stbi_image_free(decoded_data);

    } else {
      this->data = std::move(data);
    }

    if (image_info.channels == 3) {
      // Assume RGBA because many GPUs don't support RGB
      image_info.channels = 4;
    }
    if (!this->data.data()) {
      throw std::runtime_error("Failed to load image data from memory.");
    }
  }

  void TaskQueue::load_image(const ImageData& image_data, const VkImage image) {
    
      const VkDeviceSize image_size = image_data.get_image_size();

      // Step 2: Create a staging buffer
      const auto [buffer, allocation] = create_staging_buffer(image_size);

      // Copy pixel data to the staging buffer
      void* data;
      vmaMapMemory(gpu_->getAllocator(), allocation, &data);
      memcpy(data, image_data.get_data(), image_size);
      vmaUnmapMemory(gpu_->getAllocator(), allocation);

      // Step 3: Create and transition the Vulkan image
      VkImageSubresourceRange subresource_range;
      subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      subresource_range.baseMipLevel = 0;
      subresource_range.levelCount = 1;
      subresource_range.baseArrayLayer = 0;
      subresource_range.layerCount = 1;

      VkImageMemoryBarrier barrier_to_transfer = {};
      barrier_to_transfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      barrier_to_transfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      barrier_to_transfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      barrier_to_transfer.srcAccessMask = 0;
      barrier_to_transfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      barrier_to_transfer.image = image;
      barrier_to_transfer.subresourceRange = subresource_range;

      vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           0, 0, nullptr, 0, nullptr, 1, &barrier_to_transfer);

      // Step 4: Copy from the staging buffer to the Vulkan image
      VkBufferImageCopy region = {};
      region.bufferOffset = 0;
      region.bufferRowLength = 0;
      region.bufferImageHeight = 0;
      region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      region.imageSubresource.mipLevel = 0;
      region.imageSubresource.baseArrayLayer = 0;
      region.imageSubresource.layerCount = 1;
      region.imageOffset = {0, 0, 0};
      region.imageExtent = image_data.get_extent();

      vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                             &region);

      // Transition the image to the shader-readable layout
      VkImageMemoryBarrier barrier_to_shader_read = barrier_to_transfer;
      barrier_to_shader_read.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      barrier_to_shader_read.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      barrier_to_shader_read.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      barrier_to_shader_read.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

      vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                           &barrier_to_shader_read);
  }

  void TaskQueue::add_image(const std::filesystem::path& path, VkImage image) {
    enqueue([this, path, image]() {
      const ImageData image_data(path);
      load_image(image_data, image);
    });
  }

  void TaskQueue::add_image(std::vector<unsigned char>& data, VkImage image, VkExtent3D extent) {
    enqueue([this, data = std::move(data), image, extent]() {
      const ImageData image_data(data, extent);
      load_image(image_data, image);
    });
  }

  AllocatedImage ResourceAllocator::allocate_image(const std::string_view name, const VkFormat format,
                                                   const VkImageUsageFlags usage_flags, const VkExtent3D extent, const VkImageAspectFlags aspect_flags) const {
      //TODO fix image type
    VkImageCreateInfo img_info = vkinit::image_create_info(format, usage_flags, extent);
    if (extent.depth > 1) {
      img_info.imageType = VK_IMAGE_TYPE_3D;
    }

    const VmaAllocationCreateInfo allocation_info = {
      .usage = VMA_MEMORY_USAGE_GPU_ONLY,
      .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    };

    AllocatedImage image;
    VK_CHECK(vmaCreateImage(gpu_->getAllocator(), &img_info, &allocation_info, &image.image_handle,
                            &image.allocation, nullptr));
    gpu_->set_debug_name(name, VK_OBJECT_TYPE_IMAGE,
                   reinterpret_cast<uint64_t>(image.image_handle));

    VkImageViewCreateInfo view_info
        = vkinit::imageview_create_info(format, image.image_handle, aspect_flags);
    if (extent.depth > 1) {
      view_info.viewType = VK_IMAGE_VIEW_TYPE_3D; 
    }
    VK_CHECK(vkCreateImageView(gpu_->getDevice(), &view_info, nullptr, &image.image_view));

    return image;
  }

  AllocatedBuffer ResourceAllocator::allocate_buffer(const std::string_view name, const size_t size,
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

  void ResourceAllocator::destroy_buffer(const std::shared_ptr<BufferInstance>& buffer) const {
    vmaDestroyBuffer(gpu_->getAllocator(), buffer->get_buffer_handle(), buffer->get_allocation());
  }

}  // namespace gestalt