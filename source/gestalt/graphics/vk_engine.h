#pragma once

#include <vector>
#include <deque>
#include <functional>
#include <ranges>

#include "vk_types.h"
#include "vk_descriptors.h"
#include "vk_loader.h"
#include "camera.h"
#include "time_tracker.h"
#include "input_manager.h"
#include "deletion_queue.h"

struct frame_data {
  VkSemaphore swapchain_semaphore, render_semaphore;
  VkFence render_fence;

  VkCommandPool command_pool;
  VkCommandBuffer main_command_buffer;

  deletion_queue deletion_queue;
  DescriptorAllocatorGrowable frame_descriptors;
};

struct engine_stats {
  float frametime;
  int triangle_count;
  int drawcall_count;
  float scene_update_time;
  float mesh_draw_time;
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
  bool _isInitialized{false};
  int _frameNumber{0};

  VkExtent2D _windowExtent{1920, 1080};

  SDL_Window* _window{nullptr};

  VkInstance _instance;
  VkDebugUtilsMessengerEXT _debug_messenger;
  VkPhysicalDevice _chosenGPU;
  VkDevice _device;

  frame_data frames[FRAME_OVERLAP];

  frame_data& get_current_frame() { return frames[_frameNumber % FRAME_OVERLAP]; }

  VkQueue _graphicsQueue;
  uint32_t _graphicsQueueFamily;

  VkSurfaceKHR _surface;
  VkSwapchainKHR _swapchain;
  VkFormat _swapchainImageFormat;
  VkExtent2D _swapchainExtent;
  VkExtent2D _drawExtent;
  float renderScale = 1.f;

  DescriptorAllocatorGrowable globalDescriptorAllocator;

  VkPipeline _gradientPipeline;
  VkPipelineLayout _gradientPipelineLayout;

  std::vector<VkFramebuffer> _framebuffers;
  std::vector<VkImage> _swapchainImages;
  std::vector<VkImageView> _swapchainImageViews;

  VkDescriptorSet _drawImageDescriptors;
  VkDescriptorSetLayout _drawImageDescriptorLayout;

  VkDescriptorSetLayout _singleImageDescriptorLayout;

  GPUSceneData sceneData;

  std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loadedScenes;

  VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;

  deletion_queue main_deletion_queue_;

  VmaAllocator _allocator;  // vma lib allocator

  GPUMeshBuffers rectangle;

  MaterialInstance defaultData;
  GLTFMetallic_Roughness metalRoughMaterial;

  draw_context mainDrawContext;
  std::unordered_map<std::string, std::shared_ptr<Node>> loadedNodes;

  // immediate submit structures
  VkFence _immFence;
  VkCommandBuffer _imguiCommandBuffer;
  VkCommandPool _imguiCommandPool;

  // draw resources

  AllocatedImage _drawImage;
  AllocatedImage _depthImage;
  AllocatedImage _whiteImage;
  AllocatedImage _blackImage;
  AllocatedImage _greyImage;
  AllocatedImage _errorCheckerboardImage;

  VkSampler _defaultSamplerLinear;
  VkSampler _defaultSamplerNearest;

  std::vector<compute_effect> backgroundEffects;
  int currentBackgroundEffect{0};

  // initializes everything in the engine
  void init();

  // shuts down the engine
  void cleanup();

  // draw loop
  void draw();

  void draw_background(VkCommandBuffer cmd);

  void draw_main(VkCommandBuffer cmd);

  void draw_geometry(VkCommandBuffer cmd);

  void update_scene();

  void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);

  // run main loop
  void run();

  void immediate_submit(std::function<void(VkCommandBuffer cmd)> function);

  GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

  AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage,
                                VmaMemoryUsage memoryUsage);
  void destroy_buffer(const AllocatedBuffer& buffer);

  AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                              bool mipmapped = false);
  AllocatedImage create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                              bool mipmapped = false);
  void destroy_image(const AllocatedImage& img);

  std::vector<std::unique_ptr<camera_positioner_interface>> camera_positioners{1};
  uint32_t current_camera_positioner_index{0};
  camera main_camera{};

  time_tracker time_tracker;

  input_manager input_manager;

  engine_stats stats;

  bool resize_requested{false};
  bool freeze_rendering{false};

private:
  void init_vulkan();

  void init_swapchain();
  void create_swapchain(uint32_t width, uint32_t height);
  void destroy_swapchain();
  void resize_swapchain();

  void init_commands();
  void init_background_pipelines();
  void init_pipelines();
  void init_descriptors();
  void init_sync_structures();
  void init_imgui();
  void init_default_data();
  void init_renderables();
};