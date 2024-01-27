#pragma once

#include "vk_types.h"
#include "vk_gpu.h"
#include "sdl_window.h"
#include "vk_deletion_service.h"


class vk_swapchain {

  vk_gpu gpu_;
  vk_deletion_service deletion_service_;

public:

  VkSwapchainKHR swapchain;
  VkFormat swapchain_image_format;
  VkExtent2D swapchain_extent;
  VkExtent2D draw_extent;

  std::vector<VkImage> swapchain_images;
  std::vector<VkImageView> swapchain_image_views;

  void init(const vk_gpu& gpu, const sdl_window& window, frame_buffer& frame_buffer);
  void create_swapchain(uint32_t width, uint32_t height);
  void resize_swapchain(sdl_window& window);
  void destroy_swapchain();
};
