#pragma once

#include "vk_types.h"
#include "vk_gpu.h"
#include "sdl_window.h"

class vk_swapchain {
  vulkan_gpu gpu_;

public:

  VkSwapchainKHR swapchain;
  VkFormat swapchain_image_format;
  VkExtent2D swapchain_extent;
  VkExtent2D draw_extent;

  std::vector<VkImage> swapchain_images;
  std::vector<VkImageView> swapchain_image_views;

  void init(const vulkan_gpu& gpu, const sdl_window& window, AllocatedImage& draw_image,
            AllocatedImage& depth_image);
  void create_swapchain(uint32_t width, uint32_t height);
  void resize_swapchain(sdl_window& window);
  void destroy_swapchain();
};
