#pragma once

#include "Gui.hpp"
#include "vk_types.hpp"
#include "SceneManager.hpp"
#include "Window.hpp"
#include "RenderPipeline.hpp"
#include "InputSystem.hpp"
#include "Gpu.hpp"
#include "TmeTrackingService.hpp"

namespace gestalt {
  class Engine {
  public:
    void init();
    void cleanup() const;
    void run();

  private:
    bool is_initialized_{false};
    bool quit_{false};
    bool freeze_rendering_{false};

    std::shared_ptr <application::Window> window_ = std::make_shared<application::Window>();
    std::shared_ptr<graphics::Gpu> gpu_ = std::make_shared<graphics::Gpu>();

    std::shared_ptr<graphics::RenderPipeline> render_pipeline_ = std::make_shared<graphics::RenderPipeline>();
    std::shared_ptr<application::SceneManager> scene_manager_ = std::make_shared<application::SceneManager>();
    std::shared_ptr<foundation::Repository> repository_ = std::make_shared<foundation::Repository>();

    application::GuiCapabilities gui_actions_;
    std::shared_ptr<application::Gui> imgui_ = std::make_shared<application::Gui>();
    void register_gui_actions();

    // utility services
    application::TimeTrackingService time_tracking_service_;
    application::InputSystem input_system_;
    std::shared_ptr<graphics::ResourceManager> resource_manager_ = std::make_shared<graphics::ResourceManager>();

    foundation::EngineStats stats_ = {};
  };
}  // namespace gestalt