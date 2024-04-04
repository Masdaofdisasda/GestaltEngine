
#pragma once 

#include <vulkan/vulkan.h>


namespace gestalt {
  namespace foundation {
    class TextureHandle;
  }

  namespace graphics {

    namespace vkutil {

      class Transition {
      public:
        explicit Transition(const VkImage image)
          : aspectMask_(0) {
          imageBarrier.image = image;
        }

        Transition& from(VkImageLayout old_layout);

        Transition& to(const VkImageLayout new_layout);

        Transition& withAspect(const VkImageAspectFlags aspect_mask);

        Transition& withSource(VkPipelineStageFlags2 src_stage_mask, VkAccessFlags2 src_access_mask);

        Transition& withDestination(VkPipelineStageFlags2 dst_stage_mask,
                                    VkAccessFlags2 dst_access_mask);

        void andSubmitTo(const VkCommandBuffer cmd);

      private:
        VkImageAspectFlags aspectMask_;
        VkImageMemoryBarrier2 imageBarrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext = nullptr,
            .srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        };
      };

      void transition_read(VkCommandBuffer cmd, foundation::TextureHandle& image);
      void transition_write(VkCommandBuffer cmd, foundation::TextureHandle& image);
      void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout,
                            VkImageLayout newLayout);
      void copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination,
                               VkExtent2D srcSize, VkExtent2D dstSize);
      void generate_mipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize);
    };  // namespace vkutil
  }     // namespace graphics
}  // namespace gestalt