#pragma once

#include <memory>
#include <vector>

#include "Resources/ResourceTypes.hpp"
#include "Utils/CommandBuffer.hpp"
#include "Utils/Pipeline.hpp"

namespace gestalt::graphics {
  class ResourceAllocator;
}

namespace gestalt::graphics {
  struct FrameGraphNode;

  class SynchronizationManager {
    class SynchronizationVisitor final : public ResourceVisitor {
      CommandBuffer cmd_;
      std::vector<VkImageMemoryBarrier2> image_barriers;
      std::vector<VkBufferMemoryBarrier2> buffer_barriers;

      static VkPipelineStageFlags2 get_dst_stage_mask(const VkShaderStageFlags shader_stage);

    public:
      explicit SynchronizationVisitor(const CommandBuffer cmd) : cmd_(cmd) {}

      void visit(BufferInstance& buffer, const ResourceUsage usage,
                 const VkShaderStageFlags shader_stage) override;

      void visit(ImageInstance& image, ResourceUsage usage,
                 const VkShaderStageFlags shader_stage) override;

      void visit(ImageArrayInstance& images, ResourceUsage usage,
                 VkShaderStageFlags shader_stage) override;


      void apply() const;
    };

  public:
    void synchronize_resources(const std::shared_ptr<FrameGraphNode>& node,
                               const CommandBuffer cmd);
  };

}  // namespace gestalt::graphics::fg