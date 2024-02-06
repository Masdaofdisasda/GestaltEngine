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
  } filter_;

  struct blur_x {
    std::string fragment = "../shaders/ssao_blur_x.frag.spv";
    VkPipeline pipeline_;
    VkPipelineLayout pipeline_layout_;
    std::vector<VkDescriptorSetLayout> descriptor_layouts_;
  } blur_x_;

  struct blur_y {
    std::string fragment = "../shaders/ssao_blur_y.frag.spv";
    VkPipeline pipeline_;
    VkPipelineLayout pipeline_layout_;
    std::vector<VkDescriptorSetLayout> descriptor_layouts_;
  } blur_y_;

  struct final {
    std::string fragment = "../shaders/ssao_final.frag.spv";
    VkPipeline pipeline_;
    VkPipelineLayout pipeline_layout_;
    std::vector<VkDescriptorSetLayout> descriptor_layouts_;
  } final_;

  DescriptorAllocatorGrowable descriptorPool_;

  double_buffered_frame_buffer ssao_buffer_;
  AllocatedImage rotation_pattern;

  uint32_t effect_size_ = 1024;

  VkPushConstantRange push_constant_range_{
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      .offset = 0,
      .size = sizeof(render_config::ssao_params),
  };

  void prepare_filter_pass(VkShaderModule vertex_shader, VkShaderModule ssao_filter_shader);
  void prepare_blur_x(VkShaderModule vertex_shader, VkShaderModule ssao_blur_x_shader);
  void prepare_blur_y(VkShaderModule vertex_shader, VkShaderModule ssao_blur_y_shader);
  void prepare_final(VkShaderModule vertex_shader, VkShaderModule ssao_final_shader);

public:
  void prepare() override;
  void cleanup() override;
  void execute(VkCommandBuffer cmd) override;
};
