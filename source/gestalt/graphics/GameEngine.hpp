#pragma once

#include "Gpu.hpp"
#include "Window.hpp"
#include "Gui.hpp"
#include "InputSystem.hpp"
#include "Render Engine/RenderEngine.hpp"
#include "ECS/EntityComponentSystem.hpp"
#include "FrameProvider.hpp"
#include "ResourceAllocator.hpp"
#include "TmeTrackingService.hpp"

namespace gestalt {
  class GameEngine final {
  public:
    GameEngine();
    ~GameEngine();

    GameEngine(const GameEngine&) = delete;
    GameEngine& operator=(const GameEngine&) = delete;

    GameEngine(GameEngine&&) = delete;
    GameEngine& operator=(GameEngine&&) = delete;

    void run();

  private:
    bool is_initialized_{false};
    bool quit_{false};
    bool freeze_rendering_{false};
    uint64 frame_number_{0};

    application::Window window_;
    graphics::Gpu gpu_;
    foundation::Repository repository_;
    foundation::FrameProvider frame_provider_;
    graphics::ResourceAllocator resource_allocator_;
    application::EntityComponentSystem ecs_;
    graphics::RenderEngine render_engine_;
    std::unique_ptr<application::Gui> imgui_;

    application::TimeTrackingService time_tracking_service_;
    application::InputSystem input_system_;
  };
}  // namespace gestalt