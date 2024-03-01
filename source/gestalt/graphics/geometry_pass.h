#pragma once

#include "vk_types.h"
#include <string>

#include "render_pass.h"

class geometry_pass final : public render_pass {
  std::string vertex_shader_source_ = "../shaders/geometry.vert.spv";
  std::string fragment_shader_source_ = "../shaders/geometry.frag.spv";

  shader_pass_dependency_info deps_ = {
      .read_resources = {std::make_shared<depth_image_resource>("directional_shadow_map", 1.f)},
      .write_resources
      = {{"scene_opaque_color", std::make_shared<color_image_resource>("skybox_color", 1.f)},
         {"scene_opaque_depth", std::make_shared<depth_image_resource>("skybox_depth", 1.f)}},
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

class deferred_pass final : public render_pass {
  std::string vertex_shader_source_ = "../shaders/geometry.vert.spv";
  std::string fragment_shader_source_ = "../shaders/geometry_deferred.frag.spv";

  shader_pass_dependency_info deps_ = {
      .read_resources = {},
      .write_resources
      = {{"gbuffer_1_final", std::make_shared<color_image_resource>("gbuffer_1", 1.f)},
         {"gbuffer_2_final", std::make_shared<color_image_resource>("gbuffer_2", 1.f)},
         {"gbuffer_3_final", std::make_shared<color_image_resource>("gbuffer_3", 1.f)},
         {"gbuffer_depth", std::make_shared<depth_image_resource>("gbuffer_d", 1.f)}},
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

class transparent_pass final : public render_pass {
  std::string vertex_shader_source_ = "../shaders/geometry.vert.spv";
  std::string fragment_shader_source_ = "../shaders/transparent.frag.spv";

  shader_pass_dependency_info deps_ = {
      .read_resources = {std::make_shared<depth_image_resource>("directional_shadow_map", 1.f)},
      .write_resources = {{"scene_transparent_color",
                           std::make_shared<color_image_resource>("scene_opaque_color", 1.f)},
                          {"scene_transparent_depth",
                           std::make_shared<depth_image_resource>("scene_opaque_depth", 1.f)}},
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