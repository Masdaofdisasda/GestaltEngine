#pragma once
#include <functional>

struct gui_actions {

  std::function<void()> exit;
  std::function<void()> add_camera;
  std::function<engine_stats&()> get_stats;
  std::function <gpu_scene_data&()> get_scene_data;
};
