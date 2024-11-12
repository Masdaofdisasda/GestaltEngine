#pragma once
#include <memory>
#include <vector>

#include "common.hpp"
#include "VulkanTypes.hpp"
#include <Window.hpp>

namespace gestalt::foundation {
  class IGpu;
}

namespace gestalt::graphics {

    class VkSwapchain : public NonCopyable<VkSwapchain> {
    IGpu* gpu_ = nullptr;

    public:
      VkSwapchainKHR swapchain;
      VkFormat swapchain_image_format;
      VkExtent2D swapchain_extent;
      VkExtent2D draw_extent;

      std::vector<std::shared_ptr<TextureHandle>> swapchain_images;

      void init(IGpu* gpu, const VkExtent3D& extent);
      void create_swapchain(uint32_t width, uint32_t height);
      void resize_swapchain(Window* window);
      void destroy_swapchain() const;
    };


}  // namespace gestalt