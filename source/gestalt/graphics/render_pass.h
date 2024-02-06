#pragma once

#include "vk_types.h"
#include "vk_gpu.h"
#include "resource_manager.h"


class vk_renderer;

class render_pass {
public:
  void init(const vk_gpu& gpu, vk_renderer& renderer,
            const std::shared_ptr<resource_manager>& resource_manager) {
    gpu_ = gpu;
    renderer_ = &renderer;
    resource_manager_ = resource_manager;

    prepare();
  }
  virtual ~render_pass() = default;

  virtual void execute(VkCommandBuffer cmd) = 0;
  virtual void cleanup() = 0;

protected:
  virtual void prepare() = 0;

  vk_gpu gpu_ = {};
  vk_renderer* renderer_ = nullptr;
  std::shared_ptr<resource_manager> resource_manager_;

  VkPipeline pipeline_;
  VkPipelineLayout pipeline_layout_;
  std::vector<VkDescriptorSetLayout> descriptor_layouts_;
};
