﻿#pragma once

#include "vk_types.h"
#include <string>

#include "render_pass.h"

class geometry_pass final : public render_pass {
public:
  void prepare() override;
  void cleanup() override;
  void execute(VkCommandBuffer cmd) override;

private:

  std::string vertex_shader_source_ = "../shaders/geometry.vert.spv";
  std::string fragment_shader_source_ = "../shaders/geometry.frag.spv";
};

