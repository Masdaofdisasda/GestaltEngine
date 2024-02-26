#pragma once

#include "vk_types.h"
#include "render_pass.h"
#include "vk_descriptors.h"

class hdr_pass final : public render_pass {
  std::string vertex_hdr = "../shaders/fullscreen.vert.spv";

  struct bright_pass {
    std::string fragment = "../shaders/hdr_bright_pass.frag.spv";
    VkPipeline pipeline_;
    VkPipelineLayout pipeline_layout_;
    std::vector<VkDescriptorSetLayout> descriptor_layouts_;
    VkDescriptorSet descriptor_set_;
  } bright_pass_;

  struct adaptation {
    std::string fragment = "../shaders/hdr_light_adaptation.frag.spv";
    VkPipeline pipeline_;
    VkPipelineLayout pipeline_layout_;
    std::vector<VkDescriptorSetLayout> descriptor_layouts_;
    VkDescriptorSet descriptor_set_;
  } adaptation_;

  struct streaks {
    std::string fragment = "../shaders/hdr_streaks.frag.spv";
    VkPipeline pipeline_;
    VkPipelineLayout pipeline_layout_;
    std::vector<VkDescriptorSetLayout> descriptor_layouts_;
    VkDescriptorSet descriptor_set_;
  } streaks_;

  struct blur_x {
    std::string fragment = "../shaders/hdr_bloom_x.frag.spv";
    VkPipeline pipeline_;
    VkPipelineLayout pipeline_layout_;
    std::vector<VkDescriptorSetLayout> descriptor_layouts_;
    VkDescriptorSet descriptor_set_;
  } blur_x_;

  struct blur_y {
    std::string fragment = "../shaders/hdr_bloom_y.frag.spv";
    VkPipeline pipeline_;
    VkPipelineLayout pipeline_layout_;
    std::vector<VkDescriptorSetLayout> descriptor_layouts_;
    VkDescriptorSet descriptor_set_;
  } blur_y_;

  struct final {
    std::string fragment = "../shaders/hdr_final.frag.spv";
    VkPipeline pipeline_;
    VkPipelineLayout pipeline_layout_;
    std::vector<VkDescriptorSetLayout> descriptor_layouts_;
    VkDescriptorSet descriptor_set_;
  } final_;

  descriptor_writer writer;

  double_buffered_frame_buffer hdr_buffer_;
  AllocatedImage streak_pattern;

  uint32_t effect_size_ = 512;

  VkPushConstantRange push_constant_range{
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      .offset = 0,
      .size = sizeof(render_config::hdr_params),
  };

  VkViewport viewport_{
      .x = 0,
      .y = 0,
      .width = 0,
      .height = 0,
      .minDepth = 0.f,
      .maxDepth = 1.f,
  };
  VkRect2D scissor_{
      .offset = {0, 0},
      .extent = {0, 0},
  };

public:
  void prepare() override;
  void execute_blur_x(VkCommandBuffer cmd);
  void execute_blur_y(VkCommandBuffer cmd);
  void cleanup() override;
  void execute(VkCommandBuffer cmd) override;
};

class bright_pass_shader final : public shader_pass {
  std::string vertex_shader_source_ = "../shaders/fullscreen.vert.spv";
  std::string fragment_shader_source_ = "../shaders/hdr_bright_pass.frag.spv";

  uint32_t effect_size_ = 512;
  VkExtent2D extent_{effect_size_, effect_size_};

  VkPushConstantRange push_constant_range{
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      .offset = 0,
      .size = sizeof(render_config::hdr_params),
  };

  VkViewport viewport_{
      .x = 0,
      .y = 0,
      .width = static_cast<float>(effect_size_),
      .height = static_cast<float>(effect_size_),
      .minDepth = 0.f,
      .maxDepth = 1.f,
  };
  VkRect2D scissor_{
      .offset = {0, 0},
      .extent = extent_,
  };

  shader_pass_dependency_info deps_ = {.read_resources = {{std::make_shared<color_image_resource>("scene_ssao", 1.0f)}},
      .write_resources = {{"scene_brightness_filtered",
                           std::make_shared<color_image_resource>("scene_bright", extent_)}}};

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

class bloom_blur_shader final : public shader_pass {
  std::string vertex_shader_source_ = "../shaders/fullscreen.vert.spv";
  std::string fragment_blur_x = "../shaders/hdr_bloom_x.frag.spv";
  std::string fragment_blur_y = "../shaders/hdr_bloom_y.frag.spv";

  uint32_t effect_size_ = 512;
  VkExtent2D extent_{effect_size_, effect_size_};

  VkViewport viewport_{
      .x = 0,
      .y = 0,
      .width = static_cast<float>(effect_size_),
      .height = static_cast<float>(effect_size_),
      .minDepth = 0.f,
      .maxDepth = 1.f,
  };
  VkRect2D scissor_{
      .offset = {0, 0},
      .extent = extent_,
  };

  shader_pass_dependency_info deps_ = {
      .read_resources = {},
      .write_resources = {{"scene_bloom",
                           std::make_shared<color_image_resource>("scene_brightness_filtered", extent_)},
                          {"bloom_blurred_intermediate",
                           std::make_shared<color_image_resource>("bloom_blur_y", extent_)}},
  };

  descriptor_writer writer;

  VkPipeline blur_x_pipeline_ = nullptr;
  VkPipeline blur_y_pipeline_ = nullptr;
  VkPipelineLayout pipeline_layout_ = nullptr;
  std::vector<VkDescriptorSetLayout> descriptor_layouts_;
  VkDescriptorSet blur_x_descriptor_set_ = nullptr;
  VkDescriptorSet blur_y_descriptor_set_ = nullptr;

public:
  void prepare() override;
  void cleanup() override;
  void execute(VkCommandBuffer cmd) override;
  shader_pass_dependency_info& get_dependencies() override { return deps_; }
};

class tonemap_pass_shader final : public shader_pass {
  std::string vertex_shader_source_ = "../shaders/fullscreen.vert.spv";
  std::string fragment_shader_source_ = "../shaders/hdr_final.frag.spv";

  uint32_t effect_size_ = 512;
  VkExtent2D extent_{effect_size_, effect_size_};

  VkPushConstantRange push_constant_range{
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      .offset = 0,
      .size = sizeof(render_config::hdr_params),
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

  shader_pass_dependency_info deps_
      = {.read_resources = {{std::make_shared<color_image_resource>("scene_bloom", extent_)},
                            {std::make_shared<color_image_resource>("scene_ssao", extent_)}},
         .write_resources
         = {{"scene_final", std::make_shared<color_image_resource>("hdr_final", 1.0f)}}};

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