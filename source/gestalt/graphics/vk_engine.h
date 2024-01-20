#pragma once

#include <vector>
#include <deque>
#include <functional>
#include <ranges>

#include "vk_types.h"
#include "vk_descriptors.h"
#include "vk_loader.h"
#include "sdl_window.h"
#include "vk_gpu.h"
#include "vk_swapchain.h"
#include "vk_commands.h"
#include "camera.h"
#include "time_tracking_service.h"
#include "input_system.h"
#include "materials.h"
#include "vk_deletion_service.h"
#include "resource_manager.h"
#include "vk_pipelines.h"
#include "vk_sync.h"
#include "gui_actions.h"
#include "imgui_gui.h"
#include "vk_render_system.h"

class render_engine {
public:
  void init();
  void cleanup();
  void draw() { renderer_.draw(imgui_); }
  void run();

  [[nodiscard]] vk_gpu get_gpu() const { return gpu_; }
  gltf_metallic_roughness& get_metallic_roughness_material() { return renderer_.metal_rough_material_; }
  default_material& get_default_material() { return scene_manager_.default_material_; }
  VkDescriptorSetLayout& get_gpu_scene_data_layout() { return renderer_.descriptor_manager.gpu_scene_data_descriptor_layout; }
  resource_manager& get_resource_manager() { return resource_manager_; }

  gpu_mesh_buffers upload_mesh(std::span<uint32_t> indices, std::span<Vertex> vertices); //soon obsolete

private:
  bool is_initialized_{false};
  bool quit_{false};

  sdl_window window_;
  vk_gpu gpu_ = {};

  vk_render_system renderer_ = {};
  vk_scene_manager scene_manager_ = {};

  gui_actions gui_actions_;
  imgui_gui imgui_ = {};

  vk_deletion_service deletion_service_ = {};


  void update_scene();

  void immediate_submit(std::function<void(VkCommandBuffer cmd)> function);

  std::vector<std::unique_ptr<camera_positioner_interface>> camera_positioners_{1};
  uint32_t current_camera_positioner_index_{0};
  camera active_camera_{};

  // utility services
  time_tracking_service time_tracking_service_;

  input_system input_system_;

  resource_manager resource_manager_ = {};

  engine_stats stats_ = {};

  bool resize_requested_{false};
  bool freeze_rendering_{false};

  void register_gui_actions();
};