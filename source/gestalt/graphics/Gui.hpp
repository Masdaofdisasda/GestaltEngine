#pragma once

#include "vk_types.hpp"
#include "Gpu.hpp"
#include "SceneManger.hpp"

namespace gestalt::graphics {
  struct RenderConfig;
}

namespace gestalt {
  namespace application {
    enum class action {
      none,
      add_directional_light,
      add_point_light,
    };

    struct GuiCapabilities {
      std::function<void()> exit;
      std::function<void(std::string)> load_gltf;
      std::function<foundation::EngineStats&()> get_stats;
      std::function<ComponentFactory&()> get_component_factory;
      std::function<graphics::RenderConfig&()> get_render_config;
      std::function<std::shared_ptr<foundation::TextureHandle>()> get_debug_image;
    };

    class Gui {
      graphics::Gpu gpu_ = {};
      Window window_;
      std::shared_ptr<foundation::Repository> repository_;
      GuiCapabilities actions_;

      uint32_t guizmo_operation_ = 0;

      action current_action_ = action::none;
      foundation::entity selected_entity_ = foundation::invalid_entity;

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
      void display_scene_hierarchy(foundation::entity entity);
      void show_transform_component(foundation::NodeComponent& node, foundation::TransformComponent& transform);
      void show_mesh_component(foundation::MeshComponent& mesh_component);
      void show_light_component(foundation::LightComponent& light, foundation::TransformComponent& transform);
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
      VkDescriptorSetLayout descriptor_set_layout_;

      void set_debug_texture(VkImageView image_view, VkSampler sampler);

      void init(graphics::Gpu& gpu, Window& window,
                VkFormat swapchainFormat, const std::shared_ptr<foundation::Repository>& repository, GuiCapabilities
                & actions);

      void cleanup();

      void draw(VkCommandBuffer cmd, const std::shared_ptr<foundation::TextureHandle>& swapchain);

      void update(const SDL_Event& e);

      void new_frame();
    };
  }  // namespace application
}  // namespace gestalt
