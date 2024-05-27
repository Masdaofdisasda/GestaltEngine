#pragma once

#include <functional>

#include "Gui.hpp"
#include "vk_types.hpp"
#include "SceneManger.hpp"
#include "Window.hpp"
#include "FrameGraph.hpp"
#include "InputSystem.hpp"
#include "Gpu.hpp"
#include "TmeTrackingService.hpp"

namespace gestalt {
  class GameEngine {
  public:
    void init();
    void cleanup();
    void run();

  private:
    bool is_initialized_{false};
    bool quit_{false};
    bool freeze_rendering_{false};

    application::Window window_;
    graphics::Gpu gpu_ = {};
    void immediate_submit(std::function<void(VkCommandBuffer cmd)> function);

    std::shared_ptr<graphics::FrameGraph> frame_graph_ = std::make_shared<graphics::FrameGraph>();
    std::shared_ptr<application::SceneManager> scene_manager_ = std::make_shared<application::SceneManager>();
    std::shared_ptr<foundation::Repository> repository_ = std::make_shared<foundation::Repository>();

    application::GuiCapabilities gui_actions_;
    std::shared_ptr<application::Gui> imgui_ = std::make_shared<application::Gui>();
    void register_gui_actions();

    void update_scene() const;

    // utility services
    application::TimeTrackingService time_tracking_service_;
    application::InputSystem input_system_;
    std::shared_ptr<graphics::ResourceManager> resource_manager_ = std::make_shared<graphics::ResourceManager>();

    foundation::EngineStats stats_ = {};
  };
}  // namespace gestalt