#include "Swapchain.hpp"

#include <VkBootstrap.h>
#include <EngineConfiguration.hpp>
#include <Interface/IGpu.hpp>

#include "ResourceAllocator.hpp"

namespace gestalt::graphics {
  void VkSwapchain::init(IGpu* gpu, const VkExtent3D& extent) {
    gpu_ = gpu;

    create_swapchain(extent.width, extent.height);
  }

  void VkSwapchain::create_swapchain(const uint32 width, const uint32 height) {
    vkb::SwapchainBuilder swapchain_builder{gpu_->getPhysicalDevice(), gpu_->getDevice(),
                                           gpu_->getSurface()};

    swapchain_image_format = VK_FORMAT_B8G8R8A8_UNORM;

    VkPresentModeKHR desired_present_mode;
    if (useVsync()) {
      desired_present_mode = VK_PRESENT_MODE_FIFO_KHR;
    } else {
      desired_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    }

    vkb::Swapchain vkb_swapchain = swapchain_builder
                                      .set_desired_format(VkSurfaceFormatKHR{
                                          .format = swapchain_image_format,
                                          .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
                                      .set_desired_present_mode(desired_present_mode)
                                      .set_desired_extent(width, height)
                                      .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                                      .set_desired_min_image_count(2)
                                      .build()
                                      .value();

    swapchain_extent = vkb_swapchain.extent;
    // store swapchain and its related images
    swapchain = vkb_swapchain.swapchain;
    gpu_->set_debug_name("Swapchain", VK_OBJECT_TYPE_SWAPCHAIN_KHR,
                         reinterpret_cast<uint64_t>(swapchain));

    for (const auto& swapchain_image : vkb_swapchain.get_images().value()) {
      swapchain_images.emplace_back(
          SwapchainImage(swapchain_image, swapchain_image_format,
                         {swapchain_extent.width, swapchain_extent.height}));
    }

    const auto views = vkb_swapchain.get_image_views().value();

    for (size_t i = 0; i < views.size(); i++) {
      gpu_->set_debug_name("Swapchain Image View " + std::to_string(i), VK_OBJECT_TYPE_IMAGE_VIEW,
                           reinterpret_cast<uint64_t>(views[i]));
      swapchain_images[i].set_image_view(views[i]);
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
    for (auto& swapchain_image : swapchain_images) {
      swapchain_image.destroy(gpu_->getDevice());
    }
  }

}  // namespace gestalt::graphics