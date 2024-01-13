// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <vector>
#include <deque>
#include <functional>
#include <ranges>

#include "vk_descriptors.h"
#include "vk_loader.h"

struct DeletionQueue {
  std::deque<std::pair<std::function<void()>, std::string>> deletors;

  void push_function(std::function<void()>&& function, const std::string& description) {
    deletors.emplace_back(std::move(function), description);
  }

  void flush() {
    for (auto& [deletor, description] : std::ranges::reverse_view(deletors)) {
#if _DEBUG
      fmt::print("Deleting: {}\n", description);
#endif
      deletor();
    }
    deletors.clear();
  }
};

struct FrameData {
  VkSemaphore _swapchainSemaphore, _renderSemaphore;
  VkFence _renderFence;

  VkCommandPool _commandPool;
  VkCommandBuffer _mainCommandBuffer;

  DeletionQueue _deletionQueue;
  DescriptorAllocatorGrowable _frameDescriptors;
};

constexpr unsigned int FRAME_OVERLAP = 2;

struct ComputePushConstants {
  glm::vec4 data1;
  glm::vec4 data2;
  glm::vec4 data3;
  glm::vec4 data4;
};

struct GPUSceneData {
  glm::mat4 view;
  glm::mat4 proj;
  glm::mat4 viewproj;
  glm::vec4 ambientColor;
  glm::vec4 sunlightDirection;  // w for sun power
  glm::vec4 sunlightColor;
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

  void build_pipelines(VulkanEngine* engine);
  void clear_resources(VkDevice device);

  MaterialInstance write_material(VkDevice device, MaterialPass pass,
                                  const MaterialResources& resources,
                                  DescriptorAllocatorGrowable& descriptorAllocator);
};

struct MeshNode : Node {
  std::shared_ptr<MeshAsset> mesh;

  void Draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
};

struct RenderObject {
  uint32_t indexCount;
  uint32_t firstIndex;
  VkBuffer indexBuffer;

  MaterialInstance* material;

  glm::mat4 transform;
  VkDeviceAddress vertexBufferAddress;
};

struct DrawContext {
  std::vector<RenderObject> OpaqueSurfaces;
};

struct ComputeEffect {
  const char* name;

  VkPipeline pipeline;
  VkPipelineLayout layout;

  ComputePushConstants data;
};

class VulkanEngine {
public:
  bool _isInitialized{false};
  int _frameNumber{0};

  VkExtent2D _windowExtent{1700, 900};

  struct SDL_Window* _window{nullptr};

  VkInstance _instance;
  VkDebugUtilsMessengerEXT _debug_messenger;
  VkPhysicalDevice _chosenGPU;
  VkDevice _device;

  FrameData _frames[FRAME_OVERLAP];

  FrameData& get_current_frame() { return _frames[_frameNumber % FRAME_OVERLAP]; };

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

  VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;

  DeletionQueue _mainDeletionQueue;

  VmaAllocator _allocator;  // vma lib allocator

  VkPipelineLayout _meshPipelineLayout;
  VkPipeline _meshPipeline;

  GPUMeshBuffers rectangle;
  std::vector<std::shared_ptr<MeshAsset>> testMeshes;

  MaterialInstance defaultData;
  GLTFMetallic_Roughness metalRoughMaterial;

  DrawContext mainDrawContext;
  std::unordered_map<std::string, std::shared_ptr<Node>> loadedNodes;

  // immediate submit structures
  VkFence _immFence;
  VkCommandBuffer _immCommandBuffer;
  VkCommandPool _immCommandPool;

  // draw resources

  AllocatedImage _drawImage;
  AllocatedImage _depthImage;
  AllocatedImage _whiteImage;
  AllocatedImage _blackImage;
  AllocatedImage _greyImage;
  AllocatedImage _errorCheckerboardImage;

  VkSampler _defaultSamplerLinear;
  VkSampler _defaultSamplerNearest;

  std::vector<ComputeEffect> backgroundEffects;
  int currentBackgroundEffect{0};

  // initializes everything in the engine
  void init();

  // shuts down the engine
  void cleanup();

  // draw loop
  void draw();

  void draw_background(VkCommandBuffer cmd);

  void draw_geometry(VkCommandBuffer cmd);

  void update_scene();

  void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);

  // run main loop
  void run();

  void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

  GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

  AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage,
                                VmaMemoryUsage memoryUsage);
  void destroy_buffer(const AllocatedBuffer& buffer);

  AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                              bool mipmapped = false);
  AllocatedImage create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                              bool mipmapped = false);
  void destroy_image(const AllocatedImage& img);

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

  void init_mesh_pipeline();

  void init_descriptors();

  void init_sync_structures();

  void init_imgui();

  void init_default_data();
};