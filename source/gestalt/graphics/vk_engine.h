// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>

struct DeletionQueue {
  std::deque<std::function<void()>> deletors;

  void push_function(std::function<void()>&& function) { deletors.push_back(function); }

  void flush() {
    // reverse iterate the deletion queue to execute all the functions
    for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
      (*it)();  // call functors
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
        VkExtent2D _windowExtent{1700, 900};

        VkSwapchainKHR _swapchain;
        VkFormat _swapchainImageFormat;

        std::vector<VkImage> _swapchainImages;
        std::vector<VkImageView> _swapchainImageViews;
        VkExtent2D _swapchainExtent;
        AllocatedImage _drawImage;
        VkExtent2D _drawExtent;

        FrameData _frames[FRAME_OVERLAP];

        VkQueue _graphicsQueue;
        uint32_t _graphicsQueueFamily;

        DeletionQueue _mainDeletionQueue;
        VmaAllocator _allocator;

	struct SDL_Window* _window{ nullptr };

	static VulkanEngine& Get();

        FrameData& get_current_frame() { return _frames[_frameNumber % FRAME_OVERLAP]; };

	void init();
	void cleanup();
	void draw();
        void draw_background(VkCommandBuffer cmd);
	void run();

  private:
        void init_vulkan();
        void init_swapchain();
        void init_commands();
        void init_sync_structures();

        void create_swapchain(uint32_t width, uint32_t height);
        void destroy_swapchain();
};
