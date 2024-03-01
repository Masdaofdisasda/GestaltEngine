#pragma once

#include "vk_types.h"
#include <string>

#include "render_pass.h"
#include "vk_types.h"

class lighting_pass final : public render_pass {
  std::string vertex_shader_source_ = "../shaders/fullscreen.vert.spv";
  std::string fragment_shader_source_ = "../shaders/pbr_lighting.frag.spv";

  shader_pass_dependency_info deps_ = {
      .read_resources
      = {std::make_shared<color_image_resource>("gbuffer_1_final", 1.f),
          std::make_shared<color_image_resource>("gbuffer_2_final", 1.f),
        std::make_shared<color_image_resource>("gbuffer_3_final", 1.f),
          std::make_shared<depth_image_resource>("gbuffer_depth", 1.f),
          std::make_shared<depth_image_resource>("directional_shadow_map", 1.f)},
      .write_resources
      = {{"scene_shaded", std::make_shared<color_image_resource>("skybox_color", 1.f)}},
  };

  VkPushConstantRange push_constant_range_{
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      .offset = 0,
      .size = sizeof(render_config::lighting_params),
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
  shader_pass_dependency_info& get_dependencies() override { return deps_; }
};
