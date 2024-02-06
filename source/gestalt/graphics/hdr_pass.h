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
  } bright_pass_;

  struct adaptation {
    std::string fragment = "../shaders/hdr_light_adaptation.frag.spv";
    VkPipeline pipeline_;
    VkPipelineLayout pipeline_layout_;
    std::vector<VkDescriptorSetLayout> descriptor_layouts_;
  } adaptation_;

  struct streaks {
    std::string fragment = "../shaders/hdr_streaks.frag.spv";
    VkPipeline pipeline_;
    VkPipelineLayout pipeline_layout_;
    std::vector<VkDescriptorSetLayout> descriptor_layouts_;
  } streaks_;

  struct blur_x {
    std::string fragment = "../shaders/hdr_bloom_x.frag.spv";
    VkPipeline pipeline_;
    VkPipelineLayout pipeline_layout_;
    std::vector<VkDescriptorSetLayout> descriptor_layouts_;
  } blur_x_;

  struct blur_y {
    std::string fragment = "../shaders/hdr_bloom_y.frag.spv";
    VkPipeline pipeline_;
    VkPipelineLayout pipeline_layout_;
    std::vector<VkDescriptorSetLayout> descriptor_layouts_;
  } blur_y_;

  struct final {
    std::string fragment = "../shaders/hdr_final.frag.spv";
    VkPipeline pipeline_;
    VkPipelineLayout pipeline_layout_;
    std::vector<VkDescriptorSetLayout> descriptor_layouts_;
  } final_;

  DescriptorAllocatorGrowable descriptor_pool_;

  double_buffered_frame_buffer hdr_buffer_;
  AllocatedImage streak_pattern;

  uint32_t effect_size_ = 256;

  VkPushConstantRange push_constant_range{
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      .offset = 0,
      .size = sizeof(render_config::hdr_params),
  };

public:
  void prepare() override;
  void cleanup() override;
  void execute(VkCommandBuffer cmd) override;
};
