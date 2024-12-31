#define STB_IMAGE_IMPLEMENTATION
#include "TaskQueue.hpp"

#include <stb_image.h>

#include "EngineConfiguration.hpp"
#include "VulkanCheck.hpp"
#include "vk_initializers.hpp"
#include "Utils/CubemapUtils.hpp"

namespace gestalt::graphics {

  static void float24to32(int w, int h, const float* img24, float* img32) {
    const int num_pixels = w * h;
    for (int i = 0; i != num_pixels; i++) {
      *img32++ = *img24++;
      *img32++ = *img24++;
      *img32++ = *img24++;
      *img32++ = 1.0f;
    }
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

  TaskQueue::StagingBuffer TaskQueue::create_staging_buffer(VkDeviceSize size) {
    StagingBuffer staging_buffer;
    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;

    if (vmaCreateBuffer(gpu_->getAllocator(), &buffer_info, &alloc_info, &staging_buffer.buffer,
                        &staging_buffer.allocation, nullptr)
        != VK_SUCCESS) {
      throw std::runtime_error("Failed to create staging buffer for image upload.");
    }

    staging_buffers_.push_back(staging_buffer);

    return staging_buffer;
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

  TaskQueue::TaskQueue(IGpu* gpu): gpu_(gpu) {
    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = gpu_->getGraphicsQueueFamily();

    VK_CHECK(vkCreateCommandPool(gpu_->getDevice(), &pool_info, nullptr, &command_pool_));

    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = command_pool_;
    alloc_info.commandBufferCount = 1;

    VK_CHECK(vkAllocateCommandBuffers(gpu_->getDevice(), &alloc_info, &cmd));

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VK_CHECK(vkCreateFence(gpu_->getDevice(), &fenceInfo, nullptr, &flushFence));
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

  void TaskQueue::process_tasks() {

    if (tasks_.empty()) return;

    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    while (!tasks_.empty()) {
      auto task = tasks_.front();
      task();
      tasks_.pop();
    }

    // End recording
    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo commandBufferSubmitInfo = {};
    commandBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    commandBufferSubmitInfo.commandBuffer = cmd;

    VkSubmitInfo2 submitInfo2 = {};
    submitInfo2.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo2.commandBufferInfoCount = 1;
    submitInfo2.pCommandBufferInfos = &commandBufferSubmitInfo;

    VK_CHECK(vkQueueSubmit2(gpu_->getGraphicsQueue(), 1, &submitInfo2, flushFence));
    VK_CHECK(vkWaitForFences(gpu_->getDevice(), 1, &flushFence, VK_TRUE, UINT64_MAX));
    VK_CHECK(vkResetFences(gpu_->getDevice(), 1, &flushFence));
    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    for (const auto& [buffer, allocation] : staging_buffers_) {
      vmaDestroyBuffer(gpu_->getAllocator(), buffer, allocation);
    }
    staging_buffers_.clear();

  }
}  // namespace gestalt