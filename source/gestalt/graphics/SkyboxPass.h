#pragma once

#include "vk_types.h"
#include <string>

#include "FrameGraph.h"

class SkyboxPass final : public RenderPass {
  std::string vertex_shader_source_ = "../shaders/skybox.vert.spv";
  std::string fragment_shader_source_ = "../shaders/skybox.frag.spv";

  ShaderPassDependencyInfo deps_ = {
      .read_resources = {},
      .write_resources = {{"skybox_color", std::make_shared<ColorImageResource>(
                                               "scene_shaded", 1.f)},
         {"skybox_depth", std::make_shared<DepthImageResource>("gbuffer_depth", 1.f)}},
  };

  VkPushConstantRange push_constant_range_{
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      .offset = 0,
      .size = sizeof(RenderConfig::SkyboxParams),
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
  ShaderPassDependencyInfo& get_dependencies() override { return deps_; }
};

class InfiniteGridPass final : public RenderPass {
  std::string vertex_shader_source_ = "../shaders/infinite_grid.vert.spv";
  std::string fragment_shader_source_ = "../shaders/infinite_grid.frag.spv";

  ShaderPassDependencyInfo deps_ = {
      .read_resources = {},
      .write_resources = {{"grid_color", std::make_shared<ColorImageResource>(
                                               "skybox_color", 1.f)},
         {"grid_depth", std::make_shared<DepthImageResource>("skybox_depth", 1.f)}},
  };

  VkPushConstantRange push_constant_range{
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      .offset = 0,
      .size = sizeof(RenderConfig::GridParams),
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
  ShaderPassDependencyInfo& get_dependencies() override { return deps_; }
};

