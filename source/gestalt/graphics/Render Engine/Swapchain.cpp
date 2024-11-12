#include "Swapchain.hpp"

#include <VkBootstrap.h>
#include <EngineConfiguration.hpp>
#include <Interface/IGpu.hpp>
#include "Resources/TextureHandle.hpp"

namespace gestalt::graphics {
  void VkSwapchain::init(IGpu* gpu, const VkExtent3D& extent) {
    gpu_ = gpu;

    create_swapchain(extent.width, extent.height);
  }

  void VkSwapchain::create_swapchain(const uint32_t width, const uint32_t height) {
    vkb::SwapchainBuilder swapchainBuilder{gpu_->getPhysicalDevice(), gpu_->getDevice(),
                                           gpu_->getSurface()};

    swapchain_image_format = VK_FORMAT_B8G8R8A8_UNORM;

    VkPresentModeKHR desired_present_mode;
    if (useVsync()) {
      desired_present_mode = VK_PRESENT_MODE_FIFO_KHR;
    } else {
      desired_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    }

    vkb::Swapchain vkbSwapchain = swapchainBuilder
                                      .set_desired_format(VkSurfaceFormatKHR{
                                          .format = swapchain_image_format,
                                          .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
                                      .set_desired_present_mode(desired_present_mode)
                                      .set_desired_extent(width, height)
                                      .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                                      .set_desired_min_image_count(2)
                                      .build()
                                      .value();

    swapchain_extent = vkbSwapchain.extent;
    // store swapchain and its related images
    swapchain = vkbSwapchain.swapchain;

    for (const auto& _swapchainImage : vkbSwapchain.get_images().value()) {
      TextureHandle handle{};
      handle.image = _swapchainImage;
      handle.setFormat(swapchain_image_format);
      handle.setLayout(VK_IMAGE_LAYOUT_UNDEFINED);
      handle.imageExtent = {swapchain_extent.width, swapchain_extent.height, 1};

      swapchain_images.push_back(std::make_shared<TextureHandle>(handle));
    }

    const auto views = vkbSwapchain.get_image_views().value();

    for (size_t i = 0; i < views.size(); i++) {
      swapchain_images[i]->imageView = views[i];
    }
  }

  void VkSwapchain::resize_swapchain(Window* window) {
    vkDeviceWaitIdle(gpu_->getDevice());

    destroy_swapchain();

    window->update_window_size();

    create_swapchain(window->extent.width, window->extent.height);
  }

  void VkSwapchain::destroy_swapchain() const {
    vkDestroySwapchainKHR(gpu_->getDevice(), swapchain, nullptr);

    // destroy swapchain resources
    for (auto& _swapchainImage : swapchain_images) {
      vkDestroyImageView(gpu_->getDevice(), _swapchainImage->imageView, nullptr);
    }
  }

}  // namespace gestalt::graphics