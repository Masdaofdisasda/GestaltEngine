#pragma once

#include "vk_types.h"
#include <string>

#include "render_pass.h"
#include "resource_manager.h"

class skybox_pass final : public render_pass {
  std::string vertex_shader_source_ = "../shaders/skybox.vert.spv";
  std::string fragment_shader_source_ = "../shaders/skybox.frag.spv";

public:
  void prepare() override;
  void cleanup() override;
  void execute(VkCommandBuffer cmd) override;
};

