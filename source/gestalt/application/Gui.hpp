#pragma once

#include <functional>

#include "RenderConfig.hpp"
#include "vk_types.hpp"
#include "SceneManager.hpp"
#include "Window.hpp"

namespace gestalt::application {
    enum class action {
      none,
      add_directional_light,
      add_point_light,
    };

    struct GuiCapabilities {
      std::function<void()> exit;
      std::function<void(std::string)> load_gltf;
      std::function<EngineStats&()> get_stats;
      std::function<ComponentFactory&()> get_component_factory;
      std::function<RenderConfig&()> get_render_config;
      std::function<std::shared_ptr<TextureHandle>()> get_debug_image;
    };

    class Gui: public NonCopyable<Gui> {
      std::shared_ptr<IGpu> gpu_;
      std::shared_ptr<Window> window_;
      std::shared_ptr<Repository> repository_;
      GuiCapabilities actions_;
      VkDescriptorPool imguiPool_;

      uint32 guizmo_operation_ = 0;

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
      VkDescriptorSet descriptor_set_;
      VkDescriptorSetLayout descriptor_set_layout_;

      void set_debug_texture(VkImageView image_view, VkSampler sampler);

      void init(const std::shared_ptr<IGpu>& gpu, std::shared_ptr<Window>& window,
                VkFormat swapchainFormat,
                const std::shared_ptr<Repository>& repository,
                const std::unique_ptr<IDescriptorUtilFactory>& descriptor_util_factory,
                    GuiCapabilities
                & actions);

      void cleanup();

      void draw(VkCommandBuffer cmd, const std::shared_ptr<TextureHandle>& swapchain);

      void update(const SDL_Event& e);

      void new_frame();
    };
}  // namespace gestalt
