#pragma once
#include <functional>

#include "scene_components.h"

struct gui_actions {

  std::function<void()> exit;
  std::function<void()> add_camera;
  std::function<engine_stats&()> get_stats;
  std::function <per_frame_data&()> get_scene_data;
  std::function <light_component&()> get_directional_light;
  std::function <entity_component&()> get_scene_root;
  std::function <entity_component&(uint32_t)> get_scene_object;
  std::function<transform_component&(size_t)> get_transform_component;
  std::function<mesh_component&(size_t)> get_mesh_component;
  std::function<mesh_surface&(size_t)> get_surface;
  std::function<material_component&(size_t)> get_material;
  std::function<render_config&()> get_render_config;
};
