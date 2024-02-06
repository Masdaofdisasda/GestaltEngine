#pragma once

#include "vk_types.h"
#include "render_pass.h"
#include "vk_descriptors.h"

class ssao_pass final : public render_pass {
  std::string vertex_ssao = "../shaders/fullscreen.vert.spv";

  struct filter {
    std::string fragment = "../shaders/ssao_filter.frag.spv";
    VkPipeline pipeline_;
    VkPipelineLayout pipeline_layout_;
    std::vector<VkDescriptorSetLayout> descriptor_layouts_;
    VkDescriptorSet descriptor_set_;
  } filter_;

  struct blur_x {
    std::string fragment = "../shaders/ssao_blur_x.frag.spv";
    VkPipeline pipeline_;
    VkPipelineLayout pipeline_layout_;
    std::vector<VkDescriptorSetLayout> descriptor_layouts_;
    VkDescriptorSet descriptor_set_;
  } blur_x_;

  struct blur_y {
    std::string fragment = "../shaders/ssao_blur_y.frag.spv";
    VkPipeline pipeline_;
    VkPipelineLayout pipeline_layout_;
    std::vector<VkDescriptorSetLayout> descriptor_layouts_;
    VkDescriptorSet descriptor_set_;
  } blur_y_;

  struct final {
    std::string fragment = "../shaders/ssao_final.frag.spv";
    VkPipeline pipeline_;
    VkPipelineLayout pipeline_layout_;
    std::vector<VkDescriptorSetLayout> descriptor_layouts_;
    VkDescriptorSet descriptor_set_;
  } final_;

  descriptor_writer writer;

  double_buffered_frame_buffer ssao_buffer_;
  AllocatedImage rotation_pattern;

  uint32_t effect_size_ = 1024;

  VkPushConstantRange push_constant_range_{
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      .offset = 0,
      .size = sizeof(render_config::ssao_params),
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

  void prepare_filter_pass(VkShaderModule vertex_shader, VkShaderModule ssao_filter_shader);
  void prepare_blur_x(VkShaderModule vertex_shader, VkShaderModule ssao_blur_x_shader);
  void prepare_blur_y(VkShaderModule vertex_shader, VkShaderModule ssao_blur_y_shader);
  void prepare_final(VkShaderModule vertex_shader, VkShaderModule ssao_final_shader);

  void execute_filter(VkCommandBuffer cmd);
  void execute_blur_x(VkCommandBuffer cmd);
  void execute_blur_y(VkCommandBuffer cmd);
  void execute_final(VkCommandBuffer cmd);

public:
  void prepare() override;
  void cleanup() override;
  void execute(VkCommandBuffer cmd) override;
};
