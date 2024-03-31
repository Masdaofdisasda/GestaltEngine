
#pragma once

#include "vk_types.h"
#include "sdl_window.h"

class vk_gpu {
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

  void init(bool use_validation_layers, sdl_window& window,
            std::function<void(std::function<void(VkCommandBuffer)>)> immediate_submit_function);
  void cleanup();
};