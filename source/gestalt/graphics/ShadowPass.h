#pragma once

#include "FrameGraph.h"
#include "vk_types.h"

namespace gestalt {
  namespace graphics {

    class DirectionalDepthPass final : public RenderPass {
      std::string vertex_shader_source_ = "../shaders/shadow_geometry.vert.spv";
      std::string fragment_shader_source_ = "../shaders/shadow_depth.frag.spv";

      VkExtent2D effect_size_{2048, 2048};

      ShaderPassDependencyInfo deps_ = {
          .read_resources = {},
          .write_resources = {{"directional_shadow_map", std::make_shared<DepthImageResource>(
                                                             "directional_depth", effect_size_)}},
      };

      VkPushConstantRange push_constant_range_{
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
          .offset = 0,
          .size = sizeof(GpuDrawPushConstants),
      };

      VkViewport viewport_{
          .x = 0,
          .y = 0,
          .minDepth = 0.f,
          .maxDepth = 1.f,
      };
      VkRect2D scissor_{
          .offset = {0, 0},
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
      ShaderPassDependencyInfo& get_dependencies() override { return deps_; }
    };
  }  // namespace graphics
}  // namespace gestalt