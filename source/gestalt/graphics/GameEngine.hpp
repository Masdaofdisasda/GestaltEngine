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

    application::Window window_;
    std::unique_ptr<graphics::Gpu> gpu_ = std::make_unique<graphics::Gpu>();

    std::unique_ptr<graphics::RenderEngine> render_pipeline_;
    std::unique_ptr<application::ECSManager> scene_manager_
        = std::make_unique<application::ECSManager>();
    std::unique_ptr<foundation::Repository> repository_
        = std::make_unique<foundation::Repository>();

    application::GuiCapabilities gui_actions_;
    std::unique_ptr<application::Gui> imgui_ = std::make_unique<application::Gui>();
    void register_gui_actions();

    // utility services
    application::TimeTrackingService time_tracking_service_;
    application::InputSystem input_system_;
    std::unique_ptr<graphics::ResourceAllocator> resource_allocator_;

    uint64 frame_number{0};
    std::unique_ptr<foundation::FrameProvider> frame_provider_
        = std::make_unique<graphics::FrameProvider>(&frame_number);
  };
}  // namespace gestalt