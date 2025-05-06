#pragma once
#include <memory>
#include <vector>

#include "common.hpp"
#include "VulkanCore.hpp"
#include <Window.hpp>

namespace gestalt::foundation {
  class IGpu;
}

namespace gestalt::graphics {

  class SwapchainImage {
    VkImage image_;
    VkFormat format_;
    VkImageView image_view_ = VK_NULL_HANDLE;
    VkExtent2D extent_;

  public:
    SwapchainImage(const VkImage image, const VkFormat format, const VkExtent2D extent)
        : image_(image), format_(format), extent_(extent) {}

    void set_image_view(VkImageView image_view) { image_view_ = image_view; }

    [[nodiscard]] VkImage get_image() const { return image_; }
    [[nodiscard]] VkFormat get_format() const { return format_; }
    [[nodiscard]] VkImageView get_image_view() const { return image_view_; }
    [[nodiscard]] VkExtent2D get_extent() const { return extent_; }

    SwapchainImage(const SwapchainImage&) = default;
    SwapchainImage& operator=(const SwapchainImage&) = default;

    SwapchainImage(SwapchainImage&& other) = default;
    SwapchainImage& operator=(SwapchainImage&& other) = default;

    void destroy(const VkDevice device) const {
      if (image_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device, image_view_, nullptr);
        vkDestroyImage(device, image_, nullptr);
      }
    }
  };

    class VkSwapchain {
    IGpu* gpu_ = nullptr;

    public:
    VkSwapchain() = default;
      ~VkSwapchain() = default;

    VkSwapchain(const VkSwapchain&) = delete;
      VkSwapchain& operator=(const VkSwapchain&) = delete;

    VkSwapchain(VkSwapchain&& other) noexcept
          : gpu_(other.gpu_),
            swapchain(other.swapchain),
            swapchain_image_format(other.swapchain_image_format),
            swapchain_extent(other.swapchain_extent),
            draw_extent(other.draw_extent),
            swapchain_images(std::move(other.swapchain_images)) {
        other.gpu_ = nullptr;
        other.swapchain = VK_NULL_HANDLE;
      }

      VkSwapchain& operator=(VkSwapchain&& other) noexcept {
        if (this != &other) {
          gpu_ = other.gpu_;
          swapchain = other.swapchain;
          swapchain_image_format = other.swapchain_image_format;
          swapchain_extent = other.swapchain_extent;
          draw_extent = other.draw_extent;
          swapchain_images = std::move(other.swapchain_images);

          other.gpu_ = nullptr;
          other.swapchain = VK_NULL_HANDLE;
        }
        return *this;
      }

      VkSwapchainKHR swapchain;
      VkFormat swapchain_image_format;
      VkExtent2D swapchain_extent;
      VkExtent2D draw_extent;

      std::vector<SwapchainImage> swapchain_images;

      void init(IGpu* gpu, const VkExtent3D& extent);
      void create_swapchain(uint32_t width, uint32_t height);
      void resize_swapchain(Window* window);
      void destroy_swapchain() const;
    };


}  // namespace gestalt