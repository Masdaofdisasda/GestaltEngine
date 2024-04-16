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
                                        .set_desired_min_image_count(2)
                                        .build()
                                        .value();

      swapchain_extent = vkbSwapchain.extent;
      // store swapchain and its related images
      swapchain = vkbSwapchain.swapchain;

      for (const auto& _swapchainImage : vkbSwapchain.get_images().value()) {
        foundation::TextureHandle handle{};
        handle.image = _swapchainImage;
        handle.setFormat(swapchain_image_format);
        handle.setLayout(VK_IMAGE_LAYOUT_UNDEFINED);
        handle.imageExtent = {swapchain_extent.width, swapchain_extent.height, 1};

        swapchain_images.push_back(std::make_shared<foundation::TextureHandle>(handle));
      }

      const auto views = vkbSwapchain.get_image_views().value();

      for (size_t i = 0; i < views.size(); i++) {
        swapchain_images[i]->imageView = views[i];
      }
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
      for (auto& _swapchainImage : swapchain_images) {
        vkDestroyImageView(gpu_.device, _swapchainImage->imageView, nullptr);
      }
    }

  }  // namespace graphics
}  // namespace gestalt