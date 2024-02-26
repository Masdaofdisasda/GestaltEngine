#pragma once

#include "vk_types.h"
#include <string>

#include "render_pass.h"

class geometry_shader final : public shader_pass {
  std::string vertex_shader_source_ = "../shaders/geometry.vert.spv";
  std::string fragment_shader_source_ = "../shaders/geometry.frag.spv";

  shader_pass_dependency_info deps_ = {
      .read_resources = {},
      .write_resources
      = {{"scene_opaque_color", std::make_shared<color_image_resource>("skybox_color", 1.f)},
         {"scene_opaque_depth", std::make_shared<depth_image_resource>("skybox_depth", 1.f)}},
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

class transparent_shader final : public shader_pass {
  std::string vertex_shader_source_ = "../shaders/geometry.vert.spv";
  std::string fragment_shader_source_ = "../shaders/transparent.frag.spv";

  shader_pass_dependency_info deps_ = {
      .read_resources = {},
      .write_resources = {{"scene_transparent_color",
                           std::make_shared<color_image_resource>("scene_opaque_color", 1.f)},
                          {"scene_transparent_depth",
                           std::make_shared<depth_image_resource>("scene_opaque_depth", 1.f)}},
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