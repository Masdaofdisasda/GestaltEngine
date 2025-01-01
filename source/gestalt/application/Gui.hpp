#pragma once

#include <functional>

#include "RenderConfig.hpp"
#include "ECS/ECSManager.hpp"
#include "Window.hpp"
#include <SDL_events.h>

namespace gestalt::application {
    enum class action {
      none,
      add_directional_light,
      add_point_light,
    };

    struct GuiCapabilities {
      std::function<void()> exit;
      std::function<void(const std::filesystem::path&)> load_gltf;
      std::function<ComponentFactory&()> get_component_factory;
      std::function<RenderConfig&()> get_render_config;
      std::function<void(Entity)> set_active_camera;
      std::function<Entity()> get_active_camera;
    };

    class Gui: public NonCopyable<Gui> {
      IGpu* gpu_ = nullptr;
      Window* window_ = nullptr;
      Repository* repository_ = nullptr;
      GuiCapabilities actions_;
      VkDescriptorPool imguiPool_;

      uint32 guizmo_operation_ = 0;

      action current_action_ = action::none;
      Entity selected_entity_ = invalid_entity;

      bool show_scene_hierarchy_ = true;
      bool show_light_adapt_settings_ = false;
      bool show_sky_settings = false;
      bool show_grid_settings = false;
      bool show_shading_settings = false;
      bool show_tone_map_settings = false;
      bool show_guizmo_ = true;
      bool show_lights_ = false;
      bool show_cameras_ = false;

      void menu_bar();
      void lights();
      void cameras();
      void scene_graph();
      void display_scene_hierarchy(Entity entity);
      void show_transform_component(NodeComponent& node, TransformComponent& transform);
      void show_mesh_component(MeshComponent& mesh_component);
      void show_light_component(LightComponent& light, TransformComponent& transform);
      void show_camera_component(CameraComponent& camera);
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
      VkDescriptorSet descriptor_set_;

      void set_debug_texture(VkImageView image_view, VkSampler sampler);

      void init(IGpu* gpu, Window* window, VkFormat swapchain_format, Repository* repository, const GuiCapabilities& actions);

      void cleanup();

      void draw(VkCommandBuffer cmd, const VkImageView swapchain_view,
                const VkExtent2D swapchain_extent);

      void update(const SDL_Event& e);

      void new_frame();
    };
}  // namespace gestalt
