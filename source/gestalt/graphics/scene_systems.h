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
  glm::mat4 calculate_sun_view_proj(const light_component& light);
  void update_directional_lights(const std::vector<std::reference_wrapper<light_component>>& lights);
  void update_point_lights(const std::vector<std::reference_wrapper<light_component>>& lights);

public:
  void prepare() override;
  void update() override;
  void cleanup() override;
};