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
