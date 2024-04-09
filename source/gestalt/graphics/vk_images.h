
#pragma once 

#include <vulkan/vulkan.h>

#include "GpuResources.h"


namespace gestalt {

  namespace graphics {

    namespace vkutil {

      class Transition {
      public:
        explicit Transition(const std::shared_ptr<foundation::TextureHandle>& image) {
          imageBarrier.image = image->image;
          image_ = image;
        }

        Transition& to(const VkImageLayout new_layout);

        Transition& withSource(VkPipelineStageFlags2 src_stage_mask, VkAccessFlags2 src_access_mask);

        Transition& withDestination(VkPipelineStageFlags2 dst_stage_mask,
                                    VkAccessFlags2 dst_access_mask);
        Transition& toLayoutRead();
        Transition& toLayoutWrite();

        void andSubmitTo(const VkCommandBuffer cmd);

      private:
        std::shared_ptr <foundation::TextureHandle> image_;
        VkImageMemoryBarrier2 imageBarrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
      };

      void copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination,
                               VkExtent2D srcSize, VkExtent2D dstSize);
      void generate_mipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize);
    };  // namespace vkutil
  }     // namespace graphics
}  // namespace gestalt