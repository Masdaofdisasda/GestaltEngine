#pragma once

#include "common.hpp"
#include "VulkanTypes.hpp"

struct SDL_Window;

namespace gestalt::application {

    class Window : public NonCopyable<Window> {
    public:
      SDL_Window* handle;
      VkExtent2D extent{1920, 1080};

      void init();
      void create_surface(VkInstance instance, VkSurfaceKHR* surface) const;
      void update_window_size();
      void capture_mouse() const;
      void release_mouse() const;
      void cleanup() const;
    };
}  // namespace gestalt