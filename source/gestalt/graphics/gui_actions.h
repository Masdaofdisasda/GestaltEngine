#pragma once
#include <database.h>

#include <functional>

class component_archetype_factory;

struct gui_actions {

  std::function<void()> exit;
  std::function<void()> add_camera;
  std::function<void(std::string)> load_gltf;
  std::function<engine_stats&()> get_stats;
  std::function <per_frame_data&()> get_scene_data;
  std::function <database&()> get_database;
  std::function <component_archetype_factory&()> get_component_factory;
  std::function<render_config&()> get_render_config;
};
