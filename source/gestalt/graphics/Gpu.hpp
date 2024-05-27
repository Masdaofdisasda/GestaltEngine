
#pragma once

#include "vk_types.hpp"
#include "Window.hpp"

namespace gestalt {
  namespace graphics {

    class Gpu {

    public:
      VkInstance instance;
      VkDevice device;
      VmaAllocator allocator;

      VkDebugUtilsMessengerEXT debug_messenger;
      VkPhysicalDevice chosen_gpu;
      VkPhysicalDeviceProperties device_properties;
      VkQueue graphics_queue;
      uint32_t graphics_queue_family;
      VkSurfaceKHR surface;


      std::function<void(std::function<void(VkCommandBuffer)>)> immediate_submit;
      PFN_vkCmdDrawMeshTasksIndirectCountEXT vkCmdDrawMeshTasksIndirectCountEXT;

      void init(
          bool use_validation_layers, application::Window& window,
          std::function<void(std::function<void(VkCommandBuffer)>)> immediate_submit_function);
      void cleanup();
    };
    
    inline struct GpuFrame {
      void next_frame() { frame_number++; }
      [[nodiscard]] uint8_t get_current_frame() const { return frame_number % 2; }

    private:
      uint64_t frame_number{0};
    } gpu_frame;

  }  // namespace graphics
}  // namespace gestalt