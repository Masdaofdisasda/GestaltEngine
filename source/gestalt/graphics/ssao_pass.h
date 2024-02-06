#pragma once

#include "vk_types.h"
#include "render_pass.h"
#include "vk_descriptors.h"

class ssao_pass final : public render_pass {
  std::string vertex_ssao = "../shaders/fullscreen.vert.spv";

  struct ssao_filter {
    std::string fragment = "../shaders/ssao_filter.frag.spv";
    VkPipeline pipeline_;
    VkPipelineLayout pipeline_layout_;
    std::vector<VkDescriptorSetLayout> descriptor_layouts_;
  } ssao_filter_;

  struct ssao_blur_x {
    std::string fragment = "../shaders/ssao_blur_x.frag.spv";
    VkPipeline pipeline_;
    VkPipelineLayout pipeline_layout_;
    std::vector<VkDescriptorSetLayout> descriptor_layouts_;
  } ssao_blur_x_;

  struct ssao_blur_y {
    std::string fragment = "../shaders/ssao_blur_y.frag.spv";
    VkPipeline pipeline_;
    VkPipelineLayout pipeline_layout_;
    std::vector<VkDescriptorSetLayout> descriptor_layouts_;
  } ssao_blur_y_;

  struct ssao_final {
    std::string fragment = "../shaders/ssao_final.frag.spv";
    VkPipeline pipeline_;
    VkPipelineLayout pipeline_layout_;
    std::vector<VkDescriptorSetLayout> descriptor_layouts_;
  } ssao_final_;

  DescriptorAllocatorGrowable descriptorPool_;

  double_buffered_frame_buffer ssao_buffer_;
  AllocatedImage rotation_pattern;

  uint32_t effect_size_ = 1024;

public:
  void prepare() override;
  void cleanup() override;
  void execute(VkCommandBuffer cmd) override;
};
