#pragma once

#include "FrameGraph.h"
#include "vk_types.h"

namespace gestalt {
  namespace graphics {
    class DirectionalDepthPass final : public RenderPass {

      VkPushConstantRange push_constant_range_{
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
          .offset = 0,
          .size = sizeof(GpuDrawPushConstants),
      };

      descriptor_writer writer;

      VkPipeline pipeline_ = nullptr;
      VkPipelineLayout pipeline_layout_ = nullptr;
      std::vector<VkDescriptorSetLayout> descriptor_layouts_;
      VkDescriptorSet descriptor_set_ = nullptr;

    public:
      void prepare() override;
      void cleanup() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Direction Depth Pass"; }
    };
      
  }  // namespace graphics
}  // namespace gestalt