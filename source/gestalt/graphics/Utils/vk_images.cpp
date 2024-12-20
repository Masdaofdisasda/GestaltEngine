
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "vk_images.hpp"
#include "vk_initializers.hpp"

#include <vma/vk_mem_alloc.h>
#include <cmath>
namespace gestalt::graphics {
  vkutil::TransitionBuffer::TransitionBuffer(const std::shared_ptr<foundation::AllocatedBuffer>& buffer) {
    buffer_ = buffer;

    bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    bufferBarrier.srcStageMask = buffer_->currentStage;
    bufferBarrier.srcAccessMask = buffer_->currentAccess;
    bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarrier.buffer = buffer_->buffer;
    bufferBarrier.offset = 0;
    bufferBarrier.size = VK_WHOLE_SIZE;
  }

  vkutil::TransitionBuffer& vkutil::TransitionBuffer::waitForWrite() {
    bufferBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT | VK_PIPELINE_STAGE_2_HOST_BIT
                                 | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    bufferBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;

    if (buffer_->usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) {
      bufferBarrier.dstAccessMask |= VK_ACCESS_2_SHADER_WRITE_BIT;
    }
    if (buffer_->memory_usage & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      bufferBarrier.dstAccessMask |= VK_ACCESS_2_HOST_WRITE_BIT;
    }

    return *this;
  }

  vkutil::TransitionBuffer& vkutil::TransitionBuffer::waitForRead() {
    bufferBarrier.dstStageMask
        = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT
          | VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
          | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_TRANSFER_BIT;

    bufferBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;

    if (buffer_->usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) {
      bufferBarrier.dstAccessMask |= VK_ACCESS_2_SHADER_READ_BIT;
    }
    if (buffer_->usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) {
      bufferBarrier.dstAccessMask |= VK_ACCESS_2_UNIFORM_READ_BIT;
    }
    if (buffer_->usage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) {
       bufferBarrier.dstStageMask |= VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;
      bufferBarrier.dstAccessMask |= VK_ACCESS_2_INDEX_READ_BIT;
    }
    if (buffer_->usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) {
      bufferBarrier.dstStageMask |= VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
      bufferBarrier.dstAccessMask |= VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
    }
    if (buffer_->usage & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT) {
       bufferBarrier.dstStageMask |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
      bufferBarrier.dstAccessMask |= VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
    }

    return *this;
  }

  void vkutil::TransitionBuffer::andSubmitTo(const VkCommandBuffer cmd) {
    VkDependencyInfo depInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .dependencyFlags = 0,
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers = &bufferBarrier,
    };
    vkCmdPipelineBarrier2(cmd, &depInfo);

    buffer_->currentAccess = bufferBarrier.dstAccessMask;  // Update current access
    buffer_->currentStage = bufferBarrier.dstStageMask;    // Update current stage
  }

  vkutil::TransitionImage::TransitionImage(const std::shared_ptr<foundation::TextureHandle>& image) {
    image_ = image;

    imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    imageBarrier.srcAccessMask = 0;
    imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
    imageBarrier.dstAccessMask = 0;
  }

  vkutil::TransitionImage& vkutil::TransitionImage::to(const VkImageLayout new_layout) {
    imageBarrier.oldLayout = image_->getLayout();

    imageBarrier.newLayout = new_layout;

    const auto aspectMask_ = image_->getType() == foundation::TextureType::kDepth ? VK_IMAGE_ASPECT_DEPTH_BIT
                                                                      : VK_IMAGE_ASPECT_COLOR_BIT;
    imageBarrier.subresourceRange = vkinit::image_subresource_range(aspectMask_);

    return *this;
  }

  vkutil::TransitionImage& vkutil::TransitionImage::withSource(VkPipelineStageFlags2 src_stage_mask,
                                                               VkAccessFlags2 src_access_mask) {
    imageBarrier.srcStageMask = src_stage_mask;
    imageBarrier.srcAccessMask = src_access_mask;
    return *this;
  }

  vkutil::TransitionImage& vkutil::TransitionImage::withDestination(
      VkPipelineStageFlags2 dst_stage_mask, VkAccessFlags2 dst_access_mask) {
    imageBarrier.dstStageMask = dst_stage_mask;
    imageBarrier.dstAccessMask = dst_access_mask;
    return *this;
  }

  void vkutil::TransitionImage::andSubmitTo(const VkCommandBuffer cmd) {
    imageBarrier.image = image_->image;

    if (image_->getType() == foundation::TextureType::kCubeMap) {
      imageBarrier.subresourceRange.baseMipLevel = 0;
      imageBarrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
      imageBarrier.subresourceRange.baseArrayLayer = 0;
      imageBarrier.subresourceRange.layerCount = 6;
    }

    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.pNext = nullptr;

    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &imageBarrier;

    if (imageBarrier.subresourceRange.aspectMask == 0) {
      return;
    }  // image already in the desired layout

    image_->setLayout(imageBarrier.newLayout);
    vkCmdPipelineBarrier2(cmd, &depInfo);

    image_ = nullptr;
    imageBarrier = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
  }

  vkutil::TransitionImage& vkutil::TransitionImage::toLayoutRead() {
    if (image_->getType() == foundation::TextureType::kColor) {
      imageBarrier.subresourceRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
      imageBarrier.oldLayout = image_->getLayout();
      imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
                                  | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
      imageBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
      imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
      imageBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

    } else if (image_->getType() == foundation::TextureType::kDepth) {
      imageBarrier.subresourceRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_DEPTH_BIT);
      imageBarrier.oldLayout = image_->getLayout();
      imageBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
      imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
                                  | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT
                                  | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
      imageBarrier.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
      imageBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    }

    return *this;
  }

  vkutil::TransitionImage& vkutil::TransitionImage::toLayoutWrite() {
    if (image_->getType() == foundation::TextureType::kColor) {
      imageBarrier.subresourceRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
      imageBarrier.oldLayout = image_->getLayout();
      imageBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

      if (imageBarrier.oldLayout == imageBarrier.newLayout) {
        imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        imageBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
      } else {
        imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        imageBarrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
      }
      imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
      imageBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

    } else if (image_->getType() == foundation::TextureType::kDepth) {
      imageBarrier.subresourceRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_DEPTH_BIT);
      imageBarrier.oldLayout = image_->getLayout();
      imageBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

      if (imageBarrier.oldLayout == imageBarrier.newLayout) {
        imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
                                    | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        imageBarrier.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      } else {
        imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        imageBarrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
      }
      imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
      imageBarrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
      imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
                                  | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
      imageBarrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }

    return *this;
  }

  vkutil::TransitionImage& vkutil::TransitionImage::toLayoutStore() {
    if (image_->getType() == foundation::TextureType::kColor) {
      imageBarrier.subresourceRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
      imageBarrier.oldLayout = image_->getLayout();
      imageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;

      if (imageBarrier.oldLayout == imageBarrier.newLayout) {
        imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
                                    | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        imageBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
      } else {
        imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        imageBarrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
      }

      imageBarrier.dstStageMask
          = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
      imageBarrier.dstAccessMask
          = VK_ACCESS_2_SHADER_WRITE_BIT; 


    } else if (image_->getType() == foundation::TextureType::kDepth) {
      imageBarrier.subresourceRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_DEPTH_BIT);
      imageBarrier.oldLayout = image_->getLayout();
      imageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;

      if (imageBarrier.oldLayout == imageBarrier.newLayout) {
        imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
                                    | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        imageBarrier.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      } else {
        imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        imageBarrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
      }
      imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
      imageBarrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
      imageBarrier.dstStageMask
          = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;               
      imageBarrier.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;  
    
    }

    return *this;
  }

  void vkutil::CopyImage::andSubmitTo(const VkCommandBuffer cmd) const {
    VkImageBlit2 blitRegion{.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr};

    blitRegion.srcOffsets[1].x = static_cast<int32_t>(source_->imageExtent.width);
    blitRegion.srcOffsets[1].y = static_cast<int32_t>(source_->imageExtent.height);
    blitRegion.srcOffsets[1].z = 1;

    blitRegion.dstOffsets[1].x = static_cast<int32_t>(destination_->imageExtent.width);
    blitRegion.dstOffsets[1].y = static_cast<int32_t>(destination_->imageExtent.height);
    blitRegion.dstOffsets[1].z = 1;

    blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blitRegion.srcSubresource.baseArrayLayer = 0;
    blitRegion.srcSubresource.layerCount = 1;
    blitRegion.srcSubresource.mipLevel = 0;

    blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blitRegion.dstSubresource.baseArrayLayer = 0;
    blitRegion.dstSubresource.layerCount = 1;
    blitRegion.dstSubresource.mipLevel = 0;

    VkBlitImageInfo2 blitInfo{.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr};
    blitInfo.dstImage = destination_->image;
    blitInfo.dstImageLayout = destination_->getLayout();
    blitInfo.srcImage = source_->image;
    blitInfo.srcImageLayout = source_->getLayout();
    blitInfo.filter = VK_FILTER_LINEAR;
    blitInfo.regionCount = 1;
    blitInfo.pRegions = &blitRegion;

    vkCmdBlitImage2(cmd, &blitInfo);
  }

  void vkutil::generate_mipmaps(VkCommandBuffer cmd, std::shared_ptr<foundation::TextureHandle>& handle) {
    VkExtent2D imageSize = {handle->imageExtent.width, handle->imageExtent.height};
    int mipLevels = int(std::floor(
                        std::log2(std::max(handle->imageExtent.width, handle->imageExtent.height))))
                    + 1;
    for (int mip = 0; mip < mipLevels; mip++) {
      VkExtent2D halfSize = imageSize;
      halfSize.width /= 2;
      halfSize.height /= 2;

      VkImageMemoryBarrier2 imageBarrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                         .pNext = nullptr};

      imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
      imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
      imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
      imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

      imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

      VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      imageBarrier.subresourceRange = vkinit::image_subresource_range(aspectMask);
      imageBarrier.subresourceRange.levelCount = 1;
      imageBarrier.subresourceRange.baseMipLevel = mip;
      imageBarrier.image = handle->image;

      VkDependencyInfo depInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .pNext = nullptr};
      depInfo.imageMemoryBarrierCount = 1;
      depInfo.pImageMemoryBarriers = &imageBarrier;

      vkCmdPipelineBarrier2(cmd, &depInfo);

      if (mip < mipLevels - 1) {
        VkImageBlit2 blitRegion{.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr};

        blitRegion.srcOffsets[1].x = imageSize.width;
        blitRegion.srcOffsets[1].y = imageSize.height;
        blitRegion.srcOffsets[1].z = 1;

        blitRegion.dstOffsets[1].x = halfSize.width;
        blitRegion.dstOffsets[1].y = halfSize.height;
        blitRegion.dstOffsets[1].z = 1;

        blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blitRegion.srcSubresource.baseArrayLayer = 0;
        blitRegion.srcSubresource.layerCount = 1;
        blitRegion.srcSubresource.mipLevel = mip;

        blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blitRegion.dstSubresource.baseArrayLayer = 0;
        blitRegion.dstSubresource.layerCount = 1;
        blitRegion.dstSubresource.mipLevel = mip + 1;

        VkBlitImageInfo2 blitInfo{.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr};
        blitInfo.dstImage = handle->image;
        blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        blitInfo.srcImage = handle->image;
        blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        blitInfo.filter = VK_FILTER_LINEAR;
        blitInfo.regionCount = 1;
        blitInfo.pRegions = &blitRegion;

        vkCmdBlitImage2(cmd, &blitInfo);

        imageSize = halfSize;
      }
      handle->setLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    }
  }
}  // namespace gestalt::graphics