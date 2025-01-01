#include "ResourceAllocator.hpp"

#include "EngineConfiguration.hpp"
#include "VulkanCheck.hpp"
#include "vk_initializers.hpp"

namespace gestalt::graphics {

  static VkImageUsageFlags get_usage_flags(const TextureType type) {
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
    if (image_template.get_format() == VK_FORMAT_UNDEFINED) {
      throw std::runtime_error("Image format must be specified.");
    }

    const VkImageUsageFlags usage_flags = get_usage_flags(image_template.get_type());

    const auto image_size = image_template.get_image_size();
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
    if (image_template.get_image_type() == ImageType::kImage2D) {
      extent.depth = 1;
    }

    if (std::holds_alternative<std::filesystem::path>(image_template.get_initial_value())) {
      const auto path = std::get<std::filesystem::path>(image_template.get_initial_value());
      if (image_template.get_image_type() == ImageType::kCubeMap) {
        const ImageInfo image_info(path);
        uint32 face_length = image_info.get_extent().height / 2;

        const auto cube_map_face_size = VkExtent3D{face_length, face_length, 1};

        const auto allocated_image
            = allocate_image(path.filename().string(), VK_FORMAT_R32G32B32A32_SFLOAT,
                             usage_flags, cube_map_face_size,
                             VK_IMAGE_ASPECT_COLOR_BIT, ImageType::kCubeMap, true);

        task_queue_.add_image(path, allocated_image.image_handle, true, true);
        return std::make_unique<ImageInstance>(std::move(image_template), allocated_image, extent);
      }

      const ImageInfo image_info(path);

      auto allocated_image = allocate_image(
          image_template.get_name(), image_info.get_format(), usage_flags, image_info.get_extent(),
          image_template.get_aspect_flags(), image_template.get_image_type());

      task_queue_.add_image(path, allocated_image.image_handle);

      return std::make_unique<ImageInstance>(std::move(image_template), allocated_image,
                                             image_info.get_extent());
    }

    if (std::holds_alternative<std::vector<unsigned char>>(image_template.get_initial_value())) {
      auto data = std::get<std::vector<unsigned char>>(image_template.get_initial_value());
      const ImageInfo image_info(data.data(), data.size(), extent);

      auto allocated_image = allocate_image(
          image_template.get_name(), image_info.get_format(), usage_flags, image_info.get_extent(),
          image_template.get_aspect_flags(), image_template.get_image_type());

      task_queue_.add_image(data, allocated_image.image_handle, image_info.get_extent());

      return std::make_unique<ImageInstance>(std::move(image_template), allocated_image,
                                             image_info.get_extent());
    }

    auto allocated_image = allocate_image(image_template.get_name(), image_template.get_format(),
                                          usage_flags, extent, image_template.get_aspect_flags(),
                                          image_template.get_image_type());

    return std::make_unique<ImageInstance>(std::move(image_template), allocated_image, extent);
  }

  AllocatedImage ResourceAllocator::allocate_image(const std::string& name, VkFormat format,
                                                   VkImageUsageFlags usage_flags, VkExtent3D extent,
                                                   VkImageAspectFlags aspect_flags,
                                                   ImageType image_type,
                                                   bool mipmap) const {
    // 1) Create the default VkImageCreateInfo
    VkImageCreateInfo img_info;
    if (image_type == ImageType::kCubeMap) {
      img_info = vkinit::cubemap_create_info(format, usage_flags, extent);
    } else {
      img_info = vkinit::image_create_info(format, usage_flags, extent);
    }

    // 2) Customize imageType, arrayLayers, and flags based on image_type
    switch (image_type) {
      case ImageType::kImage2D:
        // 2D image (single layer)
        img_info.imageType = VK_IMAGE_TYPE_2D;  // 2D
        img_info.arrayLayers = 1;               // no extra layers
        img_info.flags = 0;                     // no special flags
        break;

      case ImageType::kImage3D:
        // 3D volume
        img_info.imageType = VK_IMAGE_TYPE_3D;
        img_info.arrayLayers = 1;  // for 3D, typically 1 layer
        img_info.flags = 0;
        break;

      case ImageType::kCubeMap:
        // Cubemap
        // Cubemaps are treated as a 2D image with 6 array layers
        img_info.imageType = VK_IMAGE_TYPE_2D;
        img_info.arrayLayers = 6;
        img_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        break;
    }
    if (mipmap) {
      img_info.mipLevels
          = static_cast<uint32_t>(std::floor(std::log2(std::max(extent.width, extent.height)))) + 1;
      img_info.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    // 3) Set up the memory allocation info
    VmaAllocationCreateInfo allocation_info{};
    allocation_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocation_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    // 4) Create the VkImage + VMA allocation
    AllocatedImage image;
    VK_CHECK(vmaCreateImage(gpu_->getAllocator(), &img_info, &allocation_info, &image.image_handle,
                            &image.allocation, nullptr));

    // 5) Give the image a debug name
    gpu_->set_debug_name(name, VK_OBJECT_TYPE_IMAGE,
                         reinterpret_cast<uint64_t>(image.image_handle));

    // 6) Create the corresponding image view
    VkImageViewCreateInfo view_info
        = vkinit::imageview_create_info(format, image.image_handle, aspect_flags);

    // Adjust the viewType to match the image type
    switch (image_type) {
      case ImageType::kImage2D:
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        break;
      case ImageType::kImage3D:
        view_info.viewType = VK_IMAGE_VIEW_TYPE_3D;
        break;
      case ImageType::kCubeMap:
        view_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        view_info.subresourceRange.layerCount = 6;
        break;
    }

    VK_CHECK(vkCreateImageView(gpu_->getDevice(), &view_info, nullptr, &image.image_view));

    gpu_->set_debug_name(std::string(name + " View"), VK_OBJECT_TYPE_IMAGE_VIEW,
                         reinterpret_cast<uint64_t>(image.image_view));

    return image;
  }


  AllocatedBuffer ResourceAllocator::allocate_buffer(
      const std::string_view name, const VkDeviceSize size, VkBufferUsageFlags usage_flags,
      const VkBufferCreateFlags create_flags, const VkMemoryPropertyFlags memory_property_flags,
      const VmaAllocationCreateFlags allocation_flags, const VmaMemoryUsage memory_usage) const {
    usage_flags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    // Define the buffer create info structure
    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = create_flags,
        .size = size,
        .usage = usage_flags,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    // Define the allocation create info structure
    VmaAllocationCreateInfo allocation_info = {
        .flags = allocation_flags,
        .usage = memory_usage,
        .requiredFlags = memory_property_flags,
    };

    AllocatedBuffer buffer;
    VK_CHECK(vmaCreateBuffer(gpu_->getAllocator(), &buffer_info, &allocation_info,
                             &buffer.buffer_handle, &buffer.allocation, &buffer.info));
    gpu_->set_debug_name(name, VK_OBJECT_TYPE_BUFFER,
                         reinterpret_cast<uint64_t>(buffer.buffer_handle));

    // Get the device address for the buffer
    const VkBufferDeviceAddressInfo device_address_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = buffer.buffer_handle,
    };
    buffer.address = vkGetBufferDeviceAddress(gpu_->getDevice(), &device_address_info);

    return buffer;
  }

  std::shared_ptr<BufferInstance> ResourceAllocator::create_buffer(
      BufferTemplate&& buffer_template) const {
    auto allocated_buffer = allocate_buffer(
        buffer_template.get_name(), buffer_template.get_size(), buffer_template.get_usage(),
        buffer_template.get_buffer_create_flags(), buffer_template.get_memory_properties(),
        buffer_template.get_alloc_flags(), buffer_template.get_memory_usage());

    return std::make_shared<BufferInstance>(std::move(buffer_template), allocated_buffer);
  }

  void ResourceAllocator::destroy_buffer(const std::shared_ptr<BufferInstance>& buffer) const {
    vmaDestroyBuffer(gpu_->getAllocator(), buffer->get_buffer_handle(), buffer->get_allocation());
  }

}  // namespace gestalt