
#pragma once

#include "vk_types.h"
#include "sdl_window.h"

class vulkan_gpu {
public:

  VkInstance instance;
  VkDevice device;
  VmaAllocator allocator;

  VkDebugUtilsMessengerEXT debug_messenger;
  VkPhysicalDevice chosen_gpu;
  VkQueue graphics_queue;
  uint32_t graphics_queue_family;
  VkSurfaceKHR surface;

  void init(bool use_validation_layers, sdl_window& window);
  void cleanup();
};