#pragma once

#include "vk_types.h"
#include "Gpu.h"
#include "GpuResources.h"
#include "Window.h"


namespace gestalt {
  namespace graphics {

    class VkSwapchain {
      Gpu gpu_;

    public:
      VkSwapchainKHR swapchain;
      VkFormat swapchain_image_format;
      VkExtent2D swapchain_extent;
      VkExtent2D draw_extent;

      std::vector<std::shared_ptr<foundation::TextureHandle>> swapchain_images;
      std::vector<VkImageView> swapchain_image_views;

      void init(const Gpu& gpu, const VkExtent3D& extent);
      void create_swapchain(uint32_t width, uint32_t height);
      void resize_swapchain(application::Window& window);
      void destroy_swapchain() const;
    };
  }  // namespace graphics
}  // namespace gestalt