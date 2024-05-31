#include "Engine.hpp"

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

#include <VkBootstrap.h>

#include <tracy/Tracy.hpp>

#include "vk_types.hpp"

#include <chrono>
#include <thread>
#include <utility>

namespace gestalt {

  Engine* engine = nullptr;

  void Engine::init() {
    assert(engine == nullptr);
    engine = this;

    // todo load from file
    foundation::Config config{};
    config.useValidationLayers = true;
    foundation::EngineConfiguration::getInstance().setConfig(config);

    window_->init();

    gpu_->init(window_);

    resource_manager_->init(gpu_, repository_);

    scene_manager_->init(gpu_, resource_manager_,
                         std::make_unique<graphics::DescriptorUtilFactory>(), repository_);

    render_pipeline_->init(gpu_, window_, resource_manager_, repository_, imgui_);

    register_gui_actions();
    imgui_->init(gpu_, window_, render_pipeline_->get_swapchain_format(), repository_,
                 std::make_unique<graphics::DescriptorUtilFactory>(), gui_actions_);

    // everything went fine
    is_initialized_ = true;
  }

  void Engine::register_gui_actions() {
    gui_actions_.exit = [this]() { quit_ = true; };
    gui_actions_.load_gltf
        = [this](const std::string& path) { scene_manager_->request_scene(path); };
    gui_actions_.get_stats = [this]() -> foundation::EngineStats& { return stats_; };
    gui_actions_.get_component_factory = [this]() -> application::ComponentFactory& {
      return scene_manager_->get_component_factory();
    };
    gui_actions_.get_render_config
        = [this]() -> graphics::RenderConfig& { return render_pipeline_->get_config(); };
    gui_actions_.get_debug_image = [this]() -> std::shared_ptr<foundation::TextureHandle> {
      return render_pipeline_->get_debug_image();
    };
  }

  void Engine::run() {
    // begin clock
    const auto start = std::chrono::system_clock::now();  // todo replace with timetracker

    time_tracking_service_.update_timer();

    SDL_Event e;

    while (!quit_) {
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

      FrameMark;
    }

    // get clock again, compare with start clock
    const auto end = std::chrono::system_clock::now();

    // convert to microseconds (integer), and then come back to miliseconds
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    stats_.frametime = elapsed.count() / 1000.f;
  }

  void Engine::cleanup() const {
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