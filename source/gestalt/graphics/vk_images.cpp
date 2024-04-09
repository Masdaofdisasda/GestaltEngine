
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <vk_images.h>
#include <vk_initializers.h>

#include "GpuResources.h"

namespace gestalt {

  namespace graphics {
    using namespace foundation;

    vkutil::Transition& vkutil::Transition::to(const VkImageLayout new_layout) {
      imageBarrier.oldLayout = image_->getLayout();

      imageBarrier.newLayout = new_layout;
      image_->setLayout(new_layout);

      const auto aspectMask_ = (new_layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
                                || new_layout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL)
                                 ? VK_IMAGE_ASPECT_DEPTH_BIT
                                 : VK_IMAGE_ASPECT_COLOR_BIT;
      imageBarrier.subresourceRange = vkinit::image_subresource_range(aspectMask_);

      return *this;
    }

    vkutil::Transition& vkutil::Transition::withSource(VkPipelineStageFlags2 src_stage_mask,
                                                       VkAccessFlags2 src_access_mask) {
      imageBarrier.srcStageMask = src_stage_mask;
      imageBarrier.srcAccessMask = src_access_mask;
      return *this;
    }

    vkutil::Transition& vkutil::Transition::withDestination(VkPipelineStageFlags2 dst_stage_mask,
                                                            VkAccessFlags2 dst_access_mask) {
      imageBarrier.dstStageMask = dst_stage_mask;
      imageBarrier.dstAccessMask = dst_access_mask;
      return *this;
    }

    void vkutil::Transition::andSubmitTo(const VkCommandBuffer cmd) {

      VkDependencyInfo depInfo{};
      depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
      depInfo.pNext = nullptr;

      depInfo.imageMemoryBarrierCount = 1;
      depInfo.pImageMemoryBarriers = &imageBarrier;

      if (imageBarrier.subresourceRange.aspectMask == 0) { return;} // image already in the desired layout
      vkCmdPipelineBarrier2(cmd, &depInfo);

      image_ = nullptr;
      imageBarrier = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    }

    vkutil::Transition& vkutil::Transition::toLayoutRead() {
      constexpr auto color_read_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      constexpr auto depth_read_layout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;

      if (image_->getType() == TextureType::kColor && image_->getLayout() != color_read_layout) {
        imageBarrier.subresourceRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
        imageBarrier.oldLayout = image_->getLayout();
        imageBarrier.newLayout = color_read_layout;
        imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        imageBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        imageBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

        image_->setLayout(color_read_layout);

      } else if (image_->getType() == TextureType::kDepth && image_->getLayout() != depth_read_layout) {
        imageBarrier.subresourceRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_DEPTH_BIT);
        imageBarrier.oldLayout = image_->getLayout();
        imageBarrier.newLayout = depth_read_layout;
        imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
                                    | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        imageBarrier.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        imageBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

        image_->setLayout(depth_read_layout);
      }

      return *this;
    }

    vkutil::Transition& vkutil::Transition::toLayoutWrite() {
      constexpr auto color_write_layout = VK_IMAGE_LAYOUT_GENERAL;
      constexpr auto depth_write_layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

      if (image_->getType() == TextureType::kColor && image_->getLayout() != color_write_layout) {
        imageBarrier.subresourceRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
        imageBarrier.oldLayout = image_->getLayout();
        imageBarrier.newLayout = color_write_layout;
        imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        imageBarrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
        imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        imageBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

        image_->setLayout(color_write_layout);

      } else if (image_->getType() == TextureType::kDepth && image_->getLayout() != depth_write_layout) {
        imageBarrier.subresourceRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_DEPTH_BIT);
        imageBarrier.oldLayout = image_->getLayout();
        imageBarrier.newLayout = depth_write_layout;
        imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        imageBarrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
        imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
                                    | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        imageBarrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        image_->setLayout(depth_write_layout);
      }

      return *this;
    }

    void vkutil::copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination,
                                     VkExtent2D srcSize, VkExtent2D dstSize) {
      VkImageBlit2 blitRegion{.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr};

      blitRegion.srcOffsets[1].x = srcSize.width;
      blitRegion.srcOffsets[1].y = srcSize.height;
      blitRegion.srcOffsets[1].z = 1;

      blitRegion.dstOffsets[1].x = dstSize.width;
      blitRegion.dstOffsets[1].y = dstSize.height;
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
      blitInfo.dstImage = destination;
      blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      blitInfo.srcImage = source;
      blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      blitInfo.filter = VK_FILTER_LINEAR;
      blitInfo.regionCount = 1;
      blitInfo.pRegions = &blitRegion;

      vkCmdBlitImage2(cmd, &blitInfo);
    }

    void vkutil::generate_mipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize) {
      int mipLevels = int(std::floor(std::log2(std::max(imageSize.width, imageSize.height)))) + 1;
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
        imageBarrier.image = image;

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
          blitInfo.dstImage = image;
          blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
          blitInfo.srcImage = image;
          blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
          blitInfo.filter = VK_FILTER_LINEAR;
          blitInfo.regionCount = 1;
          blitInfo.pRegions = &blitRegion;

          vkCmdBlitImage2(cmd, &blitInfo);

          imageSize = halfSize;
        }
      }

      // transition all mip levels into the final read_only layout
      TextureHandle handle{};
      handle.image = image;
      Transition(std::make_shared<TextureHandle>(handle))
          .to(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL).andSubmitTo(cmd);
    }
  }  // namespace graphics
}  // namespace gestalt