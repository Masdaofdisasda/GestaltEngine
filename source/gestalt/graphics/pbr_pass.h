#pragma once

#include "vk_types.h"
#include <string>

#include "render_pass.h"

class pbr_pass final : public render_pass {
public:
  void init(vk_renderer& renderer) override;
  void cleanup() override;
  void execute(VkCommandBuffer cmd) override;

private:
  VkPipeline opaquePipeline;  // TODO: split into opaque and transparent render passes
  VkPipelineLayout opaquePipelineLayout;
  VkPipeline transparentPipeline;
  VkPipelineLayout transparentPipelineLayout;

  std::string vertex_shader_source_ = "../shaders/mesh.vert.spv";
  std::string fragment_shader_source_ = "../shaders/mesh.frag.spv";
};


