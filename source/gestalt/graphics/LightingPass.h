#pragma once

#include "vk_types.h"
#include <string>

#include "FrameGraph.h"

namespace gestalt {
  namespace graphics {

    class LightingPass final : public RenderPass {
      std::string vertex_shader_source_ = "../shaders/fullscreen.vert.spv";
      std::string fragment_shader_source_ = "../shaders/pbr_lighting.frag.spv";

      ShaderPassDependencyInfo deps_ = {
          .read_resources = {std::make_shared<ColorImageResource>("gbuffer_1_final", 1.f),
                             std::make_shared<ColorImageResource>("gbuffer_2_final", 1.f),
                             std::make_shared<ColorImageResource>("gbuffer_3_final", 1.f),
                             std::make_shared<DepthImageResource>("gbuffer_depth", 1.f),
                             std::make_shared<DepthImageResource>("directional_shadow_map", 1.f)},
          .write_resources
          = {{"scene_shaded", std::make_shared<ColorImageResource>("gbuffer_shaded", 1.f)}},
      };

      VkPushConstantRange push_constant_range_{
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
          .offset = 0,
          .size = sizeof(foundation::RenderConfig::LightingParams),
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
  }  // namespace render
}  // namespace gestalt
