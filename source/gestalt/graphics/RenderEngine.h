#pragma once

#include <vector>
#include <functional>

#include "ApplicationGui.h"
#include "vk_types.h"
#include "SceneManger.h"
#include "ApplicationWindow.h"
#include "FrameGraph.h"
#include "InputSystem.h"
#include "Gpu.h"
#include "TmeTrackingService.h"
#include "Camera.h"

class RenderEngine {
public:
  void init();
  void cleanup();
  void run();

private:
  bool is_initialized_{false};
  bool quit_{false};
  bool freeze_rendering_{false};

  ApplicationWindow window_;
  Gpu gpu_ = {};
  void immediate_submit(std::function<void(VkCommandBuffer cmd)> function);

  std::shared_ptr<FrameGraph> frame_graph_ = std::make_shared<FrameGraph>();
  std::shared_ptr<SceneManager> scene_manager_ = std::make_shared<SceneManager>();
  std::shared_ptr<Repository> repository_ = std::make_shared<Repository>();

  GuiCapabilities gui_actions_;
  std::shared_ptr<ApplicationGui> imgui_ = std::make_shared<ApplicationGui>();
  void register_gui_actions();

  void update_scene() const;

  std::vector<std::unique_ptr<CameraPositionerInterface>> camera_positioners_{1};
  uint32_t current_camera_positioner_index_{0};
  Camera active_camera_{};

  // utility services
  TimeTrackingService time_tracking_service_;
  InputSystem input_system_;
  std::shared_ptr<ResourceManager> resource_manager_ = std::make_shared<ResourceManager>();

  EngineStats stats_ = {};
};