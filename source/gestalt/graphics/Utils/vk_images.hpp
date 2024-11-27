
#pragma once 

#include <memory>

#include "VulkanTypes.hpp"
#include "Resources/AllocatedBufferOld.hpp"

#include "Resources/TextureHandleOld.hpp"


namespace gestalt::graphics::vkutil {

  class TransitionBuffer {
  public:
    explicit TransitionBuffer(const std::shared_ptr<foundation::AllocatedBufferOld>& buffer);
    TransitionBuffer& waitForRead();
    TransitionBuffer& waitForWrite();
    void andSubmitTo(const VkCommandBuffer cmd);
  private:
    std::shared_ptr<foundation::AllocatedBufferOld> buffer_;
    VkBufferMemoryBarrier2 bufferBarrier = {};
  };

      class TransitionImage {
      public:
        explicit TransitionImage(const std::shared_ptr<foundation::TextureHandleOld>& image);

        TransitionImage& to(VkImageLayout new_layout);

        TransitionImage& withSource(VkPipelineStageFlags2 src_stage_mask,
                                    VkAccessFlags2 src_access_mask);

        TransitionImage& withDestination(VkPipelineStageFlags2 dst_stage_mask,
                                         VkAccessFlags2 dst_access_mask);
        TransitionImage& toLayoutRead();
        TransitionImage& toLayoutWrite();
        vkutil::TransitionImage& toLayoutStore();

        void andSubmitTo(const VkCommandBuffer cmd);

      private:
        std::shared_ptr<foundation::TextureHandleOld> image_;
        VkImageMemoryBarrier2 imageBarrier{
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        };
      };

      class CopyImage {
      public:
        explicit CopyImage(const std::shared_ptr<foundation::TextureHandleOld>& source) {
          source_ = source;
        }

        CopyImage& toImage(const std::shared_ptr<foundation::TextureHandleOld>& destination) {
          destination_ = destination;

          return *this;
        }

        void andSubmitTo(const VkCommandBuffer cmd) const;

      private:
        std::shared_ptr<foundation::TextureHandleOld> source_;
        std::shared_ptr<foundation::TextureHandleOld> destination_;
      };

      void generate_mipmaps(VkCommandBuffer cmd, std::shared_ptr<foundation::TextureHandleOld>& handle);

}  // namespace gestalt