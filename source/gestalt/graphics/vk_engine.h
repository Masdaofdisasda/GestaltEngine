// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <vk_descriptors.h>

#include <ranges>
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
  VkCommandPool _commandPool;
  VkCommandBuffer _mainCommandBuffer;
  VkSemaphore _swapchainSemaphore, _renderSemaphore;
  VkFence _renderFence;
  DeletionQueue _deletionQueue;
};

struct ComputePushConstants {
  glm::vec4 data1;
  glm::vec4 data2;
  glm::vec4 data3;
  glm::vec4 data4;
};

struct ComputeEffect {
  const char* name;

  VkPipeline pipeline;
  VkPipelineLayout layout;

  ComputePushConstants data;
};

constexpr unsigned int FRAME_OVERLAP = 2;

class VulkanEngine {
public:
        VkInstance _instance;                       // Vulkan library handle
        VkDebugUtilsMessengerEXT _debug_messenger;  // Vulkan debug output handle
        VkPhysicalDevice _chosenGPU;                // GPU chosen as the default device
        VkDevice _device;                           // Vulkan device for commands
        VkSurfaceKHR _surface;                      // Vulkan window surface

	bool _isInitialized{ false };
	int _frameNumber {0};
        bool stop_rendering{false};
        bool resize_requested{false}; 
        VkExtent2D _windowExtent{1700, 900};

        VkSwapchainKHR _swapchain;
        VkFormat _swapchainImageFormat;

        std::vector<VkImage> _swapchainImages;
        std::vector<VkImageView> _swapchainImageViews;
        VkExtent2D _swapchainExtent;
        AllocatedImage _drawImage;
        AllocatedImage _depthImage;
        VkExtent2D _drawExtent;
        float renderScale = 1.f;

        FrameData _frames[FRAME_OVERLAP];

        VkQueue _graphicsQueue;
        uint32_t _graphicsQueueFamily;
        DescriptorAllocator globalDescriptorAllocator;

        VkDescriptorSet _drawImageDescriptors;
        VkDescriptorSetLayout _drawImageDescriptorLayout;

        VkPipeline _gradientPipeline;
        VkPipelineLayout _gradientPipelineLayout;
        VkPipeline _meshPipeline;
        VkPipelineLayout _meshPipelineLayout;

        std::vector<std::shared_ptr<MeshAsset>> testMeshes;

        DeletionQueue _mainDeletionQueue;
        VmaAllocator _allocator;

        // immediate submit structures
        VkFence _immFence;
        VkCommandBuffer _immCommandBuffer;
        VkCommandPool _immCommandPool;
        std::vector<ComputeEffect> backgroundEffects;
        int currentBackgroundEffect{0};

	struct SDL_Window* _window{ nullptr };

	static VulkanEngine& Get();

        FrameData& get_current_frame() { return _frames[_frameNumber % FRAME_OVERLAP]; }
        void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);
        AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage,
                                      VmaMemoryUsage memoryUsage);
        void destroy_buffer(const AllocatedBuffer& buffer);
        GPUMeshBuffers uploadMesh(std::span<uint32_t> indices,
                                                std::span<Vertex> vertices);

	void init();
	void cleanup();
	void draw();
	void run();

  private:
        void init_vulkan();
        void init_swapchain();
        void resize_swapchain();
        void init_commands();
        void init_sync_structures();
        void init_descriptors();
        void init_pipelines();
        void init_mesh_pipeline();
        void init_background_pipelines();
        void init_imgui();
        void init_default_data();

        void draw_background(VkCommandBuffer cmd);
        void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);
        void draw_geometry(VkCommandBuffer cmd);

        void create_swapchain(uint32_t width, uint32_t height);
        void destroy_swapchain();
};
