#include "GameEngine.hpp"

#include <tracy/Tracy.hpp>

#include <chrono>

#include "fmt/core.h"

namespace gestalt {

  GameEngine::GameEngine()
      : gpu_(window_),
        frame_provider_(&frame_number_),
        resource_allocator_(gpu_),
        ecs_(gpu_, resource_allocator_, repository_, event_bus_, frame_provider_),
        render_engine_(gpu_, window_, resource_allocator_, repository_, imgui_.get(), frame_provider_)
  {
    imgui_ = std::make_unique<application::Gui>(
        gpu_, window_, render_engine_.get_swapchain_format(), repository_, event_bus_,
        application::GuiCapabilities{
            [&] { quit_ = true; },
            [&](const std::filesystem::path& file_path) { ecs_.request_scene(file_path); },
            [&]() -> application::ComponentFactory& { return ecs_.get_component_factory(); },
            [&]() -> graphics::RenderConfig& { return render_engine_.get_config(); },
            [&](foundation::Entity camera) { ecs_.set_active_camera(camera); },
            [&]() -> foundation::Entity { return ecs_.get_active_camera(); }}
        );

    is_initialized_ = true;
    fmt::print("Engine initialized\n");
  }

  void GameEngine::run() {
    fmt::print("Render loop starts\n");

    time_tracking_service_.update_timer();

    SDL_Event e;

    while (!quit_) {
      input_system_.reset_frame();
      while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_QUIT) quit_ = true;

        if (e.type == SDL_KEYDOWN) {
          if (e.key.keysym.scancode == SDL_SCANCODE_F1) window_.capture_mouse();
          if (e.key.keysym.scancode == SDL_SCANCODE_F2) window_.release_mouse();
        }
        input_system_.handle_event(e, window_.get_width(), window_.get_height());

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

      time_tracking_service_.update_timer();
      render_engine_.get_config().luminance_params.time_coeff
          = time_tracking_service_.get_delta_time();

      ecs_.update_scene(
          time_tracking_service_.get_delta_time(), input_system_.get_movement(),
          static_cast<float>(window_.get_width()) / static_cast<float>(window_.get_height()));

      render_engine_.execute_passes();

      frame_number_++;
      FrameMark;
    }
  }

  GameEngine::~GameEngine() {
    fmt::print("Engine shutting down\n");
    if (is_initialized_) {
      vkDeviceWaitIdle(gpu_.getDevice());
    }
  }
}  // namespace gestalt