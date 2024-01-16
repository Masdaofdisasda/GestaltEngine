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

struct default_material {
  AllocatedImage white_image;
  AllocatedImage black_image;
  AllocatedImage grey_image;
  AllocatedImage error_checkerboard_image;

  VkSampler default_sampler_linear;
  VkSampler default_sampler_nearest;
};

constexpr unsigned int FRAME_OVERLAP = 2;


struct mesh_node : Node {
  std::shared_ptr<MeshAsset> mesh;

  void Draw(const glm::mat4& topMatrix, draw_context& ctx) override;
};

struct render_object {
  uint32_t index_count;
  uint32_t first_index;
  VkBuffer index_buffer;

  MaterialInstance* material;
  Bounds bounds;
  glm::mat4 transform;
  VkDeviceAddress vertex_buffer_address;
};

struct draw_context {
  std::vector<render_object> opaque_surfaces;
  std::vector<render_object> transparent_surfaces;
};

class render_engine {
public:
  void init();
  void cleanup();
  void draw();
  void run();

  [[nodiscard]] vk_gpu get_gpu() const { return gpu_; }
  gltf_metallic_roughness& get_metallic_roughness_material() { return metal_rough_material_; }
  default_material& get_default_material() { return default_material_; }
  AllocatedImage& get_draw_image() { return draw_image_; }
  AllocatedImage& get_depth_image() { return depth_image_; }
  VkDescriptorSetLayout& get_gpu_scene_data_layout() { return descriptor_manager_.gpu_scene_data_descriptor_layout; }
  resource_manager& get_resource_manager() { return resource_manager_; }

  GPUMeshBuffers upload_mesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

private:
  bool is_initialized_{false};
  bool quit_{false};
  int frame_number_{0};

  sdl_window window_;
  vk_gpu gpu_;
  vk_swapchain swapchain_;
  vk_command commands_;
  vk_sync sync_;
  vk_descriptor_manager descriptor_manager_;
  vk_pipeline_manager pipeline_manager_;
  gui_actions gui_actions_;
  imgui_gui imgui_;

  std::vector<frame_data> frames_{FRAME_OVERLAP};

  frame_data& get_current_frame() { return frames_[frame_number_ % FRAME_OVERLAP]; }


  std::vector<VkFramebuffer> framebuffers_;

  GPUSceneData scene_data_;

  std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loaded_scenes_;

  vk_deletion_service deletion_service_;

  GPUMeshBuffers rectangle_;

  MaterialInstance default_data_;
  gltf_metallic_roughness metal_rough_material_;

  draw_context main_draw_context_;
  std::unordered_map<std::string, std::shared_ptr<Node>> loaded_nodes_;

  // draw resources
  AllocatedImage draw_image_;
  AllocatedImage depth_image_;

  default_material default_material_;

  int current_background_effect_{0};


  void draw_background(VkCommandBuffer cmd);

  void draw_main(VkCommandBuffer cmd);

  void draw_geometry(VkCommandBuffer cmd);

  void update_scene();

  void immediate_submit(std::function<void(VkCommandBuffer cmd)> function);

  std::vector<std::unique_ptr<camera_positioner_interface>> camera_positioners_{1};
  uint32_t current_camera_positioner_index_{0};
  camera active_camera_{};

  // utility services
  time_tracking_service time_tracking_service_;

  input_system input_system_;

  resource_manager resource_manager_;

  engine_stats stats_;

  bool resize_requested_{false};
  bool freeze_rendering_{false};

  void register_gui_actions();
  void init_default_data();
  void init_renderables();
};