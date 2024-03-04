#pragma once

#include "resource_manager.h"
#include "vk_gpu.h"

class scene_system {
public:
  void init(const vk_gpu& gpu, const std::shared_ptr<resource_manager>& resource_manager) {
    gpu_ = gpu;
    resource_manager_ = resource_manager;

    prepare();
  }
  virtual ~scene_system() = default;

  virtual void update() = 0;
  virtual void cleanup() = 0;

protected:
  virtual void prepare() = 0;

  vk_gpu gpu_ = {};
  std::shared_ptr<resource_manager> resource_manager_;
};

class light_system final : public scene_system {
    const uint32_t max_point_lights_ = 256;

public:
  void prepare() override;
  void update() override;
  void cleanup() override;
};