#pragma once

#include "Gpu.hpp"
#include "Gui.hpp"
#include "InputSystem.hpp"
#include "RenderEngine.hpp"
#include "EntityManager.hpp"
#include "TmeTrackingService.hpp"
#include "Window.hpp"

namespace gestalt {
  class GameEngine {
  public:
    void init();
    void cleanup() const;
    void run();

  private:
    bool is_initialized_{false};
    bool quit_{false};
    bool freeze_rendering_{false};

    std::unique_ptr<application::Window> window_ = std::make_unique<application::Window>();
    std::unique_ptr<graphics::Gpu> gpu_ = std::make_unique<graphics::Gpu>();

    std::unique_ptr<graphics::RenderEngine> render_pipeline_
        = std::make_unique<graphics::RenderEngine>();
    std::unique_ptr<application::EntityManager> scene_manager_
        = std::make_unique<application::EntityManager>();
    std::unique_ptr<foundation::Repository> repository_
        = std::make_unique<foundation::Repository>();

    application::GuiCapabilities gui_actions_;
    std::unique_ptr<application::Gui> imgui_ = std::make_unique<application::Gui>();
    void register_gui_actions();

    // utility services
    application::TimeTrackingService time_tracking_service_;
    application::InputSystem input_system_;
    std::unique_ptr<graphics::ResourceManager> resource_manager_
        = std::make_unique<graphics::ResourceManager>();
    std::unique_ptr<foundation::IDescriptorLayoutBuilder> descriptor_layout_builder_
        = std::make_unique<graphics::DescriptorLayoutBuilder>();
    std::unique_ptr<foundation::IDescriptorWriter> writer_
        = std::make_unique<graphics::DescriptorWriter>();

    uint64 frame_number{0};
    std::unique_ptr<foundation::FrameProvider> frame_provider_
        = std::make_unique<graphics::FrameProvider>(&frame_number);
  };
}  // namespace gestalt