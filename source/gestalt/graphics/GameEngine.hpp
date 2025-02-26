#pragma once

#include "Gpu.hpp"
#include "Window.hpp"
#include "Gui.hpp"
#include "InputSystem.hpp"
#include "Render Engine/RenderEngine.hpp"
#include "ECS/ECSManager.hpp"
#include "FrameProvider.hpp"
#include "ResourceAllocator.hpp"
#include "TmeTrackingService.hpp"

namespace gestalt {
  class GameEngine {
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
    application::ECSManager ecs_;

    std::unique_ptr<graphics::RenderEngine> render_pipeline_;

    application::GuiCapabilities gui_actions_;
    std::unique_ptr<application::Gui> imgui_ = std::make_unique<application::Gui>();
    void register_gui_actions();

    // utility services
    application::TimeTrackingService time_tracking_service_;
    application::InputSystem input_system_;
  };
}  // namespace gestalt