#pragma once

#include "vk_types.h"
#include "render_pass.h"
#include "vk_descriptors.h"

class ssao_pass final : public render_pass {
  std::string vertex_shader_source_ = "../shaders/fullscreen.vert.spv";
  std::string fragment_shader_source_ = "../shaders/fullscreen.frag.spv";

  DescriptorAllocatorGrowable descriptorPool_;

public:
  void prepare() override;
  void cleanup() override;
  void execute(VkCommandBuffer cmd) override;
};
