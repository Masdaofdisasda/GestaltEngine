
#pragma once

#include "vk_types.h"
#include "Window.h"

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
      PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR;
      PFN_vkCmdDrawMeshTasksIndirectCountEXT vkCmdDrawMeshTasksIndirectCountEXT;

      void init(
          bool use_validation_layers, application::Window& window,
          std::function<void(std::function<void(VkCommandBuffer)>)> immediate_submit_function);
      void cleanup();
    };
  }  // namespace graphics
}  // namespace gestalt