#pragma once

#include <vector>
#include <functional>

#include "vk_types.h"
#include "vk_scene_manager.h"
#include "sdl_window.h"
#include "vk_gpu.h"
#include "camera.h"
#include "time_tracking_service.h"
#include "input_system.h"
#include "resource_manager.h"
#include "gui_actions.h"
#include "imgui_gui.h"
#include "vk_renderer.h"

class render_engine {
public:
  void init();
  void cleanup();
  void run();

private:
  bool is_initialized_{false};
  bool quit_{false};
  bool resize_requested_{false};
  bool freeze_rendering_{false};

  sdl_window window_;
  vk_gpu gpu_ = {};
  void immediate_submit(std::function<void(VkCommandBuffer cmd)> function);

  vk_renderer renderer_ = {};
  vk_scene_manager scene_manager_ = {};

  gui_actions gui_actions_;
  imgui_gui imgui_ = {};
  void register_gui_actions();

  void update_scene();

  std::vector<std::unique_ptr<camera_positioner_interface>> camera_positioners_{1};
  uint32_t current_camera_positioner_index_{0};
  camera active_camera_{};

  // utility services
  time_tracking_service time_tracking_service_;
  input_system input_system_;
  resource_manager resource_manager_ = {};

  engine_stats stats_ = {};


};