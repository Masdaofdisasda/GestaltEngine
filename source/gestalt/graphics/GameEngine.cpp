#include "GameEngine.hpp"

#if 0
#define VMA_DEBUG_INITIALIZE_ALLOCATIONS 1
#define VMA_DEBUG_LOG_FORMAT(format, ...) do { \
    printf((format), __VA_ARGS__); \
    printf("\n"); \
} while(false)
#endif
#define VMA_IMPLEMENTATION
#define VMA_VULKAN_VERSION 1003000
#include <vma/vk_mem_alloc.h>

#include <tracy/Tracy.hpp>

#include <chrono>

#include "fmt/core.h"

namespace gestalt {

  GameEngine* engine = nullptr;

  void GameEngine::init() {
    assert(engine == nullptr);
    engine = this;

    // todo load from file
    foundation::Config config{};
    config.useValidationLayers = true;
    foundation::EngineConfiguration::getInstance().setConfig(config);

    window_->init();

    gpu_->init(window_.get());

    resource_manager_->init(gpu_.get(), repository_.get());

    scene_manager_->init(gpu_.get(), resource_manager_.get(),
                         descriptor_layout_builder_.get(), repository_.get(), frame_provider_.get());

    render_pipeline_->init(gpu_.get(), window_.get(), resource_manager_.get(), repository_.get(),
                           imgui_.get(), frame_provider_.get());

    register_gui_actions();
    imgui_->init(gpu_.get(), window_.get(), render_pipeline_->get_swapchain_format(), repository_.get(),
                 descriptor_layout_builder_.get(), gui_actions_);

    is_initialized_ = true;
    fmt::print("Engine initialized\n");
  }

  void GameEngine::register_gui_actions() {
    gui_actions_.exit = [this]() { quit_ = true; };
    gui_actions_.load_gltf
        = [this](const std::string& path) { scene_manager_->request_scene(path); };
    gui_actions_.get_component_factory = [this]() -> application::ComponentFactory& {
      return scene_manager_->get_component_factory();
    };
    gui_actions_.get_render_config
        = [this]() -> graphics::RenderConfig& { return render_pipeline_->get_config(); };
    gui_actions_.get_debug_image = [this]() -> std::shared_ptr<foundation::TextureHandle> {
      return render_pipeline_->get_debug_image();
    };
  }

  void GameEngine::run() {
    fmt::print("Render loop starts\n");

    time_tracking_service_.update_timer();

    SDL_Event e;

    while (!quit_) {
      input_system_.reset_frame();
      while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_QUIT) quit_ = true;

        input_system_.handle_event(e, window_->extent.width, window_->extent.height);

        if (e.type == SDL_WINDOWEVENT) {
          if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
            freeze_rendering_ = true;
          }
          if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
            freeze_rendering_ = false;
          }
        }

        imgui_->update(e);
      }

      if (freeze_rendering_) {
        // throttle the speed to avoid the endless spinning
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      }

      imgui_->new_frame();

      render_pipeline_->get_config().light_adaptation.delta_time
          = time_tracking_service_.get_delta_time();

      scene_manager_->update_scene(
          time_tracking_service_.get_delta_time(), input_system_.get_movement(),
          static_cast<float>(window_->extent.width) / static_cast<float>(window_->extent.height));

      render_pipeline_->execute_passes();

      frame_number++;
      FrameMark;
    }
  }

  void GameEngine::cleanup() const {
    fmt::print("Engine shutting down\n");
    if (is_initialized_) {
      vkDeviceWaitIdle(gpu_->getDevice());

      imgui_->cleanup();
      scene_manager_->cleanup();
      render_pipeline_->cleanup();
      resource_manager_->cleanup();
      gpu_->cleanup();
      window_->cleanup();
    }
  }
}  // namespace gestalt