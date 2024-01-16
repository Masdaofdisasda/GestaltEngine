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
#include "vk_deletion_service.h"
#include "resource_manager.h"



struct engine_stats {
  float frametime;
  int triangle_count;
  int drawcall_count;
  float scene_update_time;
  float mesh_draw_time;
};

struct default_material {
  AllocatedImage white_image;
  AllocatedImage black_image;
  AllocatedImage grey_image;
  AllocatedImage error_checkerboard_image;

  VkSampler default_sampler_linear;
  VkSampler default_sampler_nearest;
};

constexpr unsigned int FRAME_OVERLAP = 2;

struct compute_push_constants {
  glm::vec4 data1;
  glm::vec4 data2;
  glm::vec4 data3;
  glm::vec4 data4;
};

struct GLTFMetallic_Roughness {
  MaterialPipeline opaquePipeline;
  MaterialPipeline transparentPipeline;

  VkDescriptorSetLayout materialLayout;

  struct MaterialConstants {
    glm::vec4 colorFactors;
    glm::vec4 metal_rough_factors;
    // padding, we need it anyway for uniform buffers
    glm::vec4 extra[14];
  };

  struct MaterialResources {
    AllocatedImage colorImage;
    VkSampler colorSampler;
    AllocatedImage metalRoughImage;
    VkSampler metalRoughSampler;
    VkBuffer dataBuffer;
    uint32_t dataBufferOffset;
  };

  DescriptorWriter writer;

  void build_pipelines(vulkan_engine* engine);
  void clear_resources(VkDevice device);

  MaterialInstance write_material(VkDevice device, MaterialPass pass,
                                  const MaterialResources& resources,
                                  DescriptorAllocatorGrowable& descriptorAllocator);
};

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

struct compute_effect {
  const char* name;

  VkPipeline pipeline;
  VkPipelineLayout layout;

  compute_push_constants data;
};

class vulkan_engine {
public:
  void init();
  void cleanup();
  void draw();
  void run();

  [[nodiscard]] vk_gpu get_gpu() const { return gpu_; }
  GLTFMetallic_Roughness& get_metallic_roughness_material() { return metal_rough_material_; }
  default_material& get_default_material() { return default_material_; }
  AllocatedImage& get_draw_image() { return draw_image_; }
  AllocatedImage& get_depth_image() { return depth_image_; }
  VkDescriptorSetLayout& get_gpu_scene_data_layout() { return gpu_scene_data_descriptor_layout_; }
  resource_manager& get_resource_manager() { return resource_manager_; }

  GPUMeshBuffers upload_mesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

private:
  bool is_initialized_{false};
  int frame_number_{0};


  sdl_window window_;

  vk_gpu gpu_;

  vk_swapchain swapchain_;

  vk_command commands_;

  std::vector<frame_data> frames_{FRAME_OVERLAP};

  frame_data& get_current_frame() { return frames_[frame_number_ % FRAME_OVERLAP]; }

  float render_scale_ = 1.f;

  DescriptorAllocatorGrowable global_descriptor_allocator_;

  VkPipeline gradient_pipeline_;
  VkPipelineLayout gradient_pipeline_layout_;

  std::vector<VkFramebuffer> framebuffers_;

  VkDescriptorSet draw_image_descriptors_;
  VkDescriptorSetLayout draw_image_descriptor_layout_;

  VkDescriptorSetLayout single_image_descriptor_layout_;

  GPUSceneData scene_data_;

  std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loaded_scenes_;

  VkDescriptorSetLayout gpu_scene_data_descriptor_layout_;

  vk_deletion_service main_deletion_queue_;

  GPUMeshBuffers rectangle_;

  MaterialInstance default_data_;
  GLTFMetallic_Roughness metal_rough_material_;

  draw_context main_draw_context_;
  std::unordered_map<std::string, std::shared_ptr<Node>> loaded_nodes_;

  // immediate submit structures
  VkFence imgui_fence_;

  // draw resources

  AllocatedImage draw_image_;
  AllocatedImage depth_image_;

  default_material default_material_;

  std::vector<compute_effect> background_effects_;
  int current_background_effect_{0};


  void draw_background(VkCommandBuffer cmd);

  void draw_main(VkCommandBuffer cmd);

  void draw_geometry(VkCommandBuffer cmd);

  void update_scene();

  void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);


  void immediate_submit(std::function<void(VkCommandBuffer cmd)> function);

  std::vector<std::unique_ptr<camera_positioner_interface>> camera_positioners_{1};
  uint32_t current_camera_positioner_index_{0};
  camera active_camera_{};

  time_tracking_service time_tracking_service_;

  input_system input_system_;

  resource_manager resource_manager_;

  engine_stats stats_;

  bool resize_requested_{false};
  bool freeze_rendering_{false};

  void init_commands();
  void init_background_pipelines();
  void init_pipelines();
  void init_descriptors();
  void init_sync_structures();
  void init_imgui();
  void init_default_data();
  void init_renderables();
};