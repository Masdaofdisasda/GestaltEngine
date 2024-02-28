#pragma once

#include "render_pass.h"
#include "vk_types.h"
#include "resource_manager.h"

class directional_depth_pass final : public render_pass {
  std::string vertex_shader_source_ = "../shaders/shadow_geometry.vert.spv";
  std::string fragment_shader_source_ = "../shaders/shadow_depth.frag.spv";

  VkExtent2D effect_size_{2048, 2048};

  shader_pass_dependency_info deps_ = {
      .read_resources = {},
      .write_resources = {{"directional_shadow_map", std::make_shared<depth_image_resource>(
                                                         "directional_depth", effect_size_)}},
  };

  VkPushConstantRange push_constant_range_{
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
      .offset = 0,
      .size = sizeof(GPUDrawPushConstants),
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

  VkPipeline pipeline_ = nullptr;
  VkPipelineLayout pipeline_layout_ = nullptr;
  std::vector<VkDescriptorSetLayout> descriptor_layouts_;

public:
  void prepare() override;
  void cleanup() override;
  void execute(VkCommandBuffer cmd) override;
  shader_pass_dependency_info& get_dependencies() override { return deps_; }
};
