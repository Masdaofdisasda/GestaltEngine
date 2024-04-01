#include "vk_swapchain.h"

#include <VkBootstrap.h>

namespace gestalt {
  namespace graphics {

    using namespace application;

    void VkSwapchain::init(const Gpu& gpu, const VkExtent3D& extent) {
      gpu_ = gpu;

      create_swapchain(extent.width, extent.height);
    }

    void VkSwapchain::create_swapchain(const uint32_t width, const uint32_t height) {
      vkb::SwapchainBuilder swapchainBuilder{gpu_.chosen_gpu, gpu_.device, gpu_.surface};

      swapchain_image_format = VK_FORMAT_B8G8R8A8_UNORM;

      vkb::Swapchain vkbSwapchain = swapchainBuilder
                                        //.use_default_format_selection()
                                        .set_desired_format(VkSurfaceFormatKHR{
                                            .format = swapchain_image_format,
                                            .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
                                        // use vsync present mode
                                        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
                                        .set_desired_extent(width, height)
                                        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                                        .build()
                                        .value();

      swapchain_extent = vkbSwapchain.extent;
      // store swapchain and its related images
      swapchain = vkbSwapchain.swapchain;
      swapchain_images = vkbSwapchain.get_images().value();
      swapchain_image_views = vkbSwapchain.get_image_views().value();
    }

    void VkSwapchain::resize_swapchain(Window& window) {
      vkDeviceWaitIdle(gpu_.device);

      destroy_swapchain();

      window.update_window_size();

      create_swapchain(window.extent.width, window.extent.height);
    }

    void VkSwapchain::destroy_swapchain() const {
      vkDestroySwapchainKHR(gpu_.device, swapchain, nullptr);

      // destroy swapchain resources
      for (auto& _swapchainImageView : swapchain_image_views) {
        vkDestroyImageView(gpu_.device, _swapchainImageView, nullptr);
      }
    }

  }  // namespace graphics
}  // namespace gestalt