#pragma once

#include "vk_types.h"
#include <string>

#include "render_pass.h"

class skybox_pass final : public render_pass {
  std::string vertex_shader_source_ = "../shaders/skybox.vert.spv";
  std::string fragment_shader_source_ = "../shaders/skybox.frag.spv";

  descriptor_writer writer_;
public:
  void init(vk_renderer& renderer) override;
  void cleanup() override;
  void execute(VkCommandBuffer cmd) override;
};

