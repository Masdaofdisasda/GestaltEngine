#pragma once

#include "vk_types.h"
#include "vk_deletion_service.h"
#include "vk_gpu.h"
#include "vk_swapchain.h"
#include "gui_actions.h"
#include "scene_manager.h"

enum class action {
  none,
  add_directional_light,
  add_point_light,
};

class imgui_gui {
  vk_gpu gpu_ = {};
  sdl_window window_;
  std::shared_ptr<vk_swapchain> swapchain_;
  gui_actions actions_;
  vk_deletion_service deletion_service_ = {};

  action current_action_ = action::none;
  entity selected_entity_ = invalid_entity;

  bool show_scene_hierarchy_ = true;
  bool show_render_settings_ = true;
  bool show_guizmo_ = true;
  bool show_stats_ = false;
  bool show_lights_ = false;

  void menu_bar();
  void stats();
  void lights();
  void scene_graph();
  void display_scene_hierarchy(entity entity);
  void show_scene_hierarchy_window();
  void render_settings();
  void guizmo();
  void check_file_dialog();

public:

  void init(vk_gpu& gpu, sdl_window& window, const std::shared_ptr<vk_swapchain>& swapchain, gui_actions& actions);

  void cleanup();

  void draw(VkCommandBuffer cmd, VkImageView target_image_view);

  void update(const SDL_Event& e);

  void new_frame();
};
