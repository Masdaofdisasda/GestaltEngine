#pragma once

#include "vk_types.h"
#include "DeletionService.h"
#include "Gpu.h"
#include "vk_swapchain.h"
#include "SceneManger.h"

enum class action {
  none,
  add_directional_light,
  add_point_light,
};

struct GuiCapabilities {
  std::function<void()> exit;
  std::function<void()> add_camera;
  std::function<void(std::string)> load_gltf;
  std::function<EngineStats&()> get_stats;
  std::function<PerFrameData&()> get_scene_data;
  std::function<ComponentArchetypeFactory&()> get_component_factory;
  std::function<RenderConfig&()> get_render_config;
};

class ApplicationGui {
  Gpu gpu_ = {};
  ApplicationWindow window_;
  std::shared_ptr<VkSwapchain> swapchain_;
  std::shared_ptr<Repository> repository_;
  GuiCapabilities actions_;
  DeletionService deletion_service_ = {};

  uint32_t guizmo_operation_ = 0;

  action current_action_ = action::none;
  entity selected_entity_ = invalid_entity;

  bool show_scene_hierarchy_ = true;
  bool show_light_adapt_settings_ = false;
  bool show_sky_settings = false;
  bool show_grid_settings = false;
  bool show_shading_settings = false;
  bool show_tone_map_settings = false;
  bool show_guizmo_ = true;
  bool show_stats_ = false;
  bool show_lights_ = false;

  void menu_bar();
  void stats();
  void lights();
  void scene_graph();
  void display_scene_hierarchy(entity entity);
  void show_transform_component(NodeComponent& node, TransformComponent& transform);
  void show_mesh_component(MeshComponent& mesh_component);
  void show_light_component(LightComponent& light, TransformComponent& transform);
  void show_node_component();
  void show_scene_hierarchy_window();
  void light_adaptation_settings();
  void sky_settings();
  void grid_settings();
  void shading_settings();
  void tone_map_settings();
  void guizmo();
  void check_file_dialog();

public:

  void init(Gpu& gpu, ApplicationWindow& window, const std::shared_ptr<VkSwapchain>& swapchain,
            const std::shared_ptr<Repository>& repository,
            GuiCapabilities & actions);

  void cleanup();

  void draw(VkCommandBuffer cmd, VkImageView target_image_view);

  void update(const SDL_Event& e);

  void new_frame();
};
