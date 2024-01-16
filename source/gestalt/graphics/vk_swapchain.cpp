#include "vk_swapchain.h"

#include "VkBootstrap.h"
#include "vk_deletion_service.h"
#include "vk_initializers.h"

void vk_swapchain::init(const vk_gpu& gpu, vk_deletion_service& deletion_service,
                        const sdl_window& window, AllocatedImage& draw_image,
                        AllocatedImage& depth_image) {
    gpu_ = gpu;
    deletion_service_ = deletion_service;

  create_swapchain(window.extent.width, window.extent.height);

  // draw image size will match the window
  VkExtent3D drawImageExtent = {window.extent.width, window.extent.height, 1};

  // hardcoding the draw format to 32 bit float
  draw_image.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
  draw_image.imageExtent = drawImageExtent;

  VkImageUsageFlags drawImageUsages{};
  drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
  drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  VkImageCreateInfo rimg_info
      = vkinit::image_create_info(draw_image.imageFormat, drawImageUsages, drawImageExtent);

  // for the draw image, we want to allocate it from gpu local memory
  VmaAllocationCreateInfo rimg_allocinfo = {};
  rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // allocate and create the image
  vmaCreateImage(gpu_.allocator, &rimg_info, &rimg_allocinfo, &draw_image.image,
                 &draw_image.allocation, nullptr);

  // build a image-view for the draw image to use for rendering
  VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(
      draw_image.imageFormat, draw_image.image, VK_IMAGE_ASPECT_COLOR_BIT);

  VK_CHECK(vkCreateImageView(gpu_.device, &rview_info, nullptr, &draw_image.imageView));

  depth_image.imageFormat = VK_FORMAT_D32_SFLOAT;
  depth_image.imageExtent = drawImageExtent;
  VkImageUsageFlags depthImageUsages{};
  depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

  VkImageCreateInfo dimg_info
      = vkinit::image_create_info(depth_image.imageFormat, depthImageUsages, drawImageExtent);

  // allocate and create the image
  vmaCreateImage(gpu_.allocator, &dimg_info, &rimg_allocinfo, &depth_image.image,
                 &depth_image.allocation, nullptr);

  // build a image-view for the draw image to use for rendering
  VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(
      depth_image.imageFormat, depth_image.image, VK_IMAGE_ASPECT_DEPTH_BIT);

  VK_CHECK(vkCreateImageView(gpu_.device, &dview_info, nullptr, &depth_image.imageView));

  deletion_service_.push(draw_image.imageView);
  deletion_service_.push(draw_image.image, draw_image.allocation);
  deletion_service_.push(depth_image.imageView);
  deletion_service_.push(depth_image.image, depth_image.allocation);
}

void vk_swapchain::create_swapchain(const uint32_t width, const uint32_t height) {
  vkb::SwapchainBuilder swapchainBuilder{gpu_.chosen_gpu, gpu_.device, gpu_.surface};

  swapchain_image_format = VK_FORMAT_B8G8R8A8_UNORM;

  vkb::Swapchain vkbSwapchain
      = swapchainBuilder
            //.use_default_format_selection()
            .set_desired_format(VkSurfaceFormatKHR{.format = swapchain_image_format,
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


void vk_swapchain::resize_swapchain(sdl_window& window) {
  vkDeviceWaitIdle(gpu_.device);

  destroy_swapchain();

  window.update_window_size();

  create_swapchain(window.extent.width, window.extent.height);
}

void vk_swapchain::destroy_swapchain() {
  vkDestroySwapchainKHR(gpu_.device, swapchain, nullptr);

  // destroy swapchain resources
  for (auto& _swapchainImageView : swapchain_image_views) {
    vkDestroyImageView(gpu_.device, _swapchainImageView, nullptr);
  }
}