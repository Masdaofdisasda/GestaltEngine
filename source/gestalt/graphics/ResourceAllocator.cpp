#include "ResourceAllocator.hpp"

#include <stb_image.h>

#include "EngineConfiguration.hpp"
#include "VulkanCheck.hpp"
#include "vk_initializers.hpp"
#include "Utils/CubemapUtils.hpp"

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

  static void float24to32(int w, int h, const float* img24, float* img32) {
    const int numPixels = w * h;
    for (int i = 0; i != numPixels; i++) {
      *img32++ = *img24++;
      *img32++ = *img24++;
      *img32++ = *img24++;
      *img32++ = 1.0f;
    }
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

  ImageInfo::ImageInfo(const std::filesystem::path& path) {
    if (!stbi_info(path.string().c_str(), &width, &height, &channels)) {
      throw std::runtime_error("Failed to read image info from file: " + path.string());
    }

    channels = 4;
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
    if (!decoded_data) {
      throw std::runtime_error("Failed to load image from file: " + path.string());
    }

    image_info.channels = 4;

    const size_t data_size = static_cast<size_t>(image_info.width) * image_info.height * image_info.
                             channels;
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
      if (!decoded_data) {
        throw std::runtime_error("Failed to load image data from memory.");
      }
      image_info.channels = 4;

      const size_t data_size = static_cast<size_t>(image_info.width) * image_info.height *
                               image_info.channels;
      this->data.assign(decoded_data, decoded_data + data_size);

      stbi_image_free(decoded_data);

    } else {
      this->data = std::move(data);
    }

    if (!this->data.data()) {
      throw std::runtime_error("Failed to load image data from memory.");
    }
  }

  HdrImageData::HdrImageData(const std::filesystem::path& path) : image_info(path) {
    float* decoded_data = stbi_loadf(path.string().c_str(), &image_info.width, &image_info.height, &image_info.channels, 3);
    if (!decoded_data) {
      throw std::runtime_error("Failed to load HDR image from file: " + path.string());
    }
    const size_t data_size
        = static_cast<size_t>(image_info.width) * image_info.height * image_info.channels;
    data.assign(decoded_data, decoded_data + data_size);
    stbi_image_free(decoded_data);
    if (!data.data()) {
      throw std::runtime_error("Failed to load HDR image data from file {" + path.string() + "}.");
    }
  }

  void TaskQueue::load_cubemap(const HdrImageData& image_data, const VkImage image, bool mipmap) {
    auto [w, h, d] = image_data.get_extent();
    std::vector<float> img32(w * h * 4);
    float24to32(w, h, image_data.get_data(), img32.data());  // Convert HDR format as needed

    Bitmap in(w, h, 4, eBitmapFormat_Float, img32.data());
    Bitmap out_bitmap = convertEquirectangularMapToVerticalCross(in);

    Bitmap cube = convertVerticalCrossToCubeMapFaces(out_bitmap);
    auto extend = VkExtent3D{static_cast<uint32>(cube.w_), static_cast<uint32>(cube.h_), 1};

    size_t faceWidth = cube.w_;
    size_t faceHeight = cube.h_;
    size_t numChannels = 4;  // Assuming RGBA
    size_t bytesPerFloat = sizeof(float32);

    size_t faceSizeBytes = faceWidth * faceHeight * numChannels * bytesPerFloat;
    size_t totalCubemapSizeBytes = faceSizeBytes * 6;

    // Step 2: Create a staging buffer
    const auto [buffer, allocation] = create_staging_buffer(totalCubemapSizeBytes);

    // Copy pixel data to the staging buffer
    void* data;
    vmaMapMemory(gpu_->getAllocator(), allocation, &data);
    memcpy(data, cube.data_.data(), totalCubemapSizeBytes);
    vmaUnmapMemory(gpu_->getAllocator(), allocation);

    // Step 3: Create and transition the Vulkan image
    VkImageSubresourceRange subresource_range;
    subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresource_range.baseMipLevel = 0;
    subresource_range.levelCount = 1;
    subresource_range.baseArrayLayer = 0;
    subresource_range.layerCount = 6;  // All cubemap faces

    VkImageMemoryBarrier barrier_to_transfer = {};
    barrier_to_transfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier_to_transfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier_to_transfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier_to_transfer.srcAccessMask = 0;
    barrier_to_transfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier_to_transfer.image = image;
    barrier_to_transfer.subresourceRange = subresource_range;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                         0, nullptr, 0, nullptr, 1, &barrier_to_transfer);

    for (int face = 0; face < 6; ++ face) {
      VkBufferImageCopy copy_region = {};
      copy_region.bufferOffset = faceSizeBytes * face;
      copy_region.bufferRowLength = 0;
      copy_region.bufferImageHeight = 0;

      copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      copy_region.imageSubresource.mipLevel = 0;
      copy_region.imageSubresource.baseArrayLayer = face;  // Specify the face index
      copy_region.imageSubresource.layerCount = 1;
      copy_region.imageExtent = extend;

      vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                             &copy_region);
    }

    if (mipmap) {
      uint32_t mipWidth = faceWidth;
      uint32_t mipHeight = faceHeight;

      uint32 mip_levels
          = static_cast<uint32_t>(std::floor(std::log2(std::max(faceWidth, faceHeight)))) + 1;

        // Transition this mip level (for all faces) to TRANSFER_DST_OPTIMAL
      VkImageSubresourceRange subresourceRangeDst = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                     .baseMipLevel = 0,
                                                     .levelCount = 1,
                                                     .baseArrayLayer = 0,
                                                     .layerCount = 6};

      VkImageMemoryBarrier barrierMipToDst = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                              .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                                              .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                                              .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                              .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                              .image = image,
                                              .subresourceRange = subresourceRangeDst};

      vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           0, 0, nullptr, 0, nullptr, 1, &barrierMipToDst);


      for (uint32_t mip = 1; mip < mip_levels; mip++) {
        // The previous mip is source
        uint32_t prevMip = mip - 1;

        // Transition this mip level (for all faces) to TRANSFER_DST_OPTIMAL
        VkImageSubresourceRange subresourceRangeDst = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                       .baseMipLevel = mip,
                                                       .levelCount = 1,
                                                       .baseArrayLayer = 0,
                                                       .layerCount = 6};

        VkImageMemoryBarrier barrierMipToDst = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                                .srcAccessMask = 0,
                                                .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                                                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                .image = image,
                                                .subresourceRange = subresourceRangeDst};

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrierMipToDst);

        // Mip dimensions
        uint32_t newMipWidth = (mipWidth > 1) ? mipWidth / 2 : 1;
        uint32_t newMipHeight = (mipHeight > 1) ? mipHeight / 2 : 1;

        // We'll blit with a single call per face
        for (uint32_t face = 0; face < 6; face++) {
          // Region for blit
          VkImageBlit blitRegion = {};
          blitRegion.srcSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                       .mipLevel = prevMip,
                                       .baseArrayLayer = face,
                                       .layerCount = 1};
          blitRegion.srcOffsets[0] = {0, 0, 0};
          blitRegion.srcOffsets[1]
              = {static_cast<int32_t>(mipWidth), static_cast<int32_t>(mipHeight), 1};

          blitRegion.dstSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                       .mipLevel = mip,
                                       .baseArrayLayer = face,
                                       .layerCount = 1};
          blitRegion.dstOffsets[0] = {0, 0, 0};
          blitRegion.dstOffsets[1]
              = {.x= static_cast<int32_t>(newMipWidth), .y= static_cast<int32_t>(newMipHeight), .z=
                 1};

          vkCmdBlitImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blitRegion,
                         VK_FILTER_LINEAR  // or VK_FILTER_CUBIC_EXT if supported
              );
        }

        // Transition the current mip level to TRANSFER_SRC_OPTIMAL if we still have more mips to
        // generate, otherwise transition to SHADER_READ_ONLY_OPTIMAL if this is the last one.
        VkAccessFlags dstAccessMask
            = (mip + 1 < mip_levels) ? VK_ACCESS_TRANSFER_READ_BIT : VK_ACCESS_SHADER_READ_BIT;
        VkImageMemoryBarrier barrierMipToSrc
            = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
               .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
               .dstAccessMask = dstAccessMask,
               .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
               .newLayout = (mip + 1 < mip_levels) ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
                                                  : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
               .image = image,
               .subresourceRange = subresourceRangeDst};

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             (mip + 1 < mip_levels) ? VK_PIPELINE_STAGE_TRANSFER_BIT
                                                   : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrierMipToSrc);

        // Update for next iteration
        mipWidth = newMipWidth;
        mipHeight = newMipHeight;
      }


    } else {
      // Transition the image to the shader-readable layout
      VkImageMemoryBarrier barrier_to_shader_read = barrier_to_transfer;
      barrier_to_shader_read.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      barrier_to_shader_read.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      barrier_to_shader_read.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      barrier_to_shader_read.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      barrier_to_shader_read.subresourceRange = subresource_range;

      // note: did not test this code path
       vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                        &barrier_to_shader_read);
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

  void TaskQueue::add_image(const std::filesystem::path& path, VkImage image, bool is_cubemap, bool mipmap) {
    if (is_cubemap) {
      enqueue([this, path, image, mipmap]() {
        const HdrImageData image_data(path);
        load_cubemap(image_data, image, mipmap);
      });
      return;
    }
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

  AllocatedImage ResourceAllocator::allocate_image(const std::string_view name, VkFormat format,
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
    auto allocated_buffer = allocate_buffer(buffer_template.get_name(), buffer_template.get_size(),
                                            buffer_template.get_usage(),
                                            buffer_template.get_memory_usage());

    return std::make_shared<BufferInstance>(std::move(buffer_template), allocated_buffer);
  }

  void ResourceAllocator::destroy_buffer(const std::shared_ptr<BufferInstance>& buffer) const {
    vmaDestroyBuffer(gpu_->getAllocator(), buffer->get_buffer_handle(), buffer->get_allocation());
  }

}  // namespace gestalt