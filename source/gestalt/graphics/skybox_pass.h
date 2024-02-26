#pragma once

#include "vk_types.h"
#include <string>

#include "render_pass.h"
#include "resource_manager.h"

class skybox_pass final : public render_pass {
  std::string vertex_shader_source_ = "../shaders/skybox.vert.spv";
  std::string fragment_shader_source_ = "../shaders/skybox.frag.spv";

  shader_pass_dependency_info deps_ = {
      .read_resources = {},
      .write_resources = {{"skybox_color", std::make_shared<color_image_resource>(
                                               "scene_color", 1.f)},
                          {"skybox_depth", std::make_shared<depth_image_resource>(
                                               "scene_depth", 1.f)}},
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

