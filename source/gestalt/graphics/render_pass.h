#pragma once

#include "vk_types.h"
#include "vk_gpu.h"


class vk_renderer;

class render_pass {
public:
  virtual ~render_pass() = default;

  virtual void init(vk_renderer& renderer) = 0;
  virtual void cleanup() = 0;
  virtual void execute(VkCommandBuffer cmd) = 0;
protected:
  vk_gpu gpu_ = {};
  vk_renderer* renderer_ = nullptr;

  VkPipeline pipeline_;
  VkPipelineLayout pipeline_layout_;
  std::vector<VkDescriptorSetLayout> descriptor_layouts_;
};
