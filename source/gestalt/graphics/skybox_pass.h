#pragma once

#include "vk_types.h"
#include <string>

#include "frame_graph.h"
#include "resource_manager.h"

class skybox_pass final : public render_pass {
  std::string vertex_shader_source_ = "../shaders/skybox.vert.spv";
  std::string fragment_shader_source_ = "../shaders/skybox.frag.spv";

  shader_pass_dependency_info deps_ = {
      .read_resources = {},
      .write_resources = {{"skybox_color", std::make_shared<color_image_resource>(
                                               "scene_shaded", 1.f)},
         {"skybox_depth", std::make_shared<depth_image_resource>("gbuffer_depth", 1.f)}},
  };

  VkPushConstantRange push_constant_range_{
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      .offset = 0,
      .size = sizeof(render_config::skybox_params),
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

class infinite_grid_pass final : public render_pass {
  std::string vertex_shader_source_ = "../shaders/infinite_grid.vert.spv";
  std::string fragment_shader_source_ = "../shaders/infinite_grid.frag.spv";

  shader_pass_dependency_info deps_ = {
      .read_resources = {},
      .write_resources = {{"grid_color", std::make_shared<color_image_resource>(
                                               "skybox_color", 1.f)},
         {"grid_depth", std::make_shared<depth_image_resource>("skybox_depth", 1.f)}},
  };

  VkPushConstantRange push_constant_range{
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      .offset = 0,
      .size = sizeof(render_config::grid_params),
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

